#include "user_mgr.hpp"
#include "users.hpp"
#include "file.hpp"
#include "logger.hpp"

#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <vector>

namespace sonic
{
namespace user
{

namespace
{
constexpr const char* passwdFileName = "/etc/passwd";

// The hardcoded groups in BMC
constexpr std::array<const char*, 1> predefinedGroups = {"redfish"};

long currentDate()
{
    const auto date = std::chrono::duration_cast<std::chrono::days>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    if (date > std::numeric_limits<long>::max())
    {
        return std::numeric_limits<long>::max();
    }

    if (date < std::numeric_limits<long>::min())
    {
        return std::numeric_limits<long>::min();
    }

    return date;
}

}

std::string getCSVFromVector(std::span<const std::string> vec)
{
    if (vec.empty())
    {
        return "";
    }
    return std::accumulate(std::next(vec.begin()), vec.end(), vec[0],
                           [](std::string&& val, std::string_view element) {
                               val += ',';
                               val += element;
                               return val;
                           });
}

bool UserMgr::isUserExist(const std::string& userName)
{
    if (userName.empty())
    {
        LOG_ERROR("User name is empty");
        throw std::invalid_argument("User name is empty");
    }
    if (usersList.find(userName) == usersList.end())
    {
        return false;
    }
    return true;
}

void UserMgr::throwForUserExists(const std::string& userName)
{
    if (isUserExist(userName))
    {
        LOG_ERROR("User %s already exists", userName.c_str());
        throw std::runtime_error("User already exists");
    }
}

void UserMgr::throwForUserDoesNotExist(const std::string& userName)
{
    if (!isUserExist(userName))
    {
        LOG_ERROR("User %s does not exist", userName.c_str());
        throw std::runtime_error("User does not exist");
    }
}

void UserMgr::executeUserClearFailRecords(const char* userName)
{
    executeCmd("/usr/sbin/faillock", "--user", userName, "--reset");
}

UserMgr::UserMgr(sdbusplus::asio::object_server& server, const char* path,
                 sonic::dbus_bridge::ObjectMapperService* objectMapper) :
    server(server),
    path(path),
    objectMapper_(objectMapper)
{
    // Register ObjectManager interface at /xyz/openbmc_project/user
    // This is required for BMCWeb's getManagedObjects() to work
    server.add_manager(path);

    // Register xyz.openbmc_project.User.Manager interface
    userMgrIface = server.add_interface(path, "xyz.openbmc_project.User.Manager");

    // Register AllPrivileges property (read-only)
    userMgrIface->register_property("AllPrivileges", privMgr);

    // Register AllGroups property (read-only)
    userMgrIface->register_property("AllGroups", allGroups);

    // Register CreateUser method
    userMgrIface->register_method(
        "CreateUser",
        [this](const std::string& userName,
               const std::vector<std::string>& groupNames,
               const std::string& priv, bool enabled) {
            createUser(userName, groupNames, priv, enabled);
        });

    // Register GetUserInfo method
    userMgrIface->register_method(
        "GetUserInfo",
        [this](const std::string& userName) {
            return getUserInfo(userName);
        });

    // Register DeleteUser method (delete via object path, not this interface)
    // BMCWeb uses the Delete method on individual user objects

    userMgrIface->initialize();

    initUserObjects();
}

void UserMgr::syncHostPasswdFiles()
{
    // Sync passwd/shadow/group from /host/etc to container's /etc
    // so that subsequent reads (PAM auth, user listing) see the updates
    // Use C++ file I/O instead of system() for security
    auto copyFile = [](const std::string& src, const std::string& dst) -> bool {
        std::ifstream srcFile(src, std::ios::binary);
        if (!srcFile)
        {
            return false;
        }
        std::ofstream dstFile(dst, std::ios::binary | std::ios::trunc);
        if (!dstFile)
        {
            return false;
        }
        dstFile << srcFile.rdbuf();
        return srcFile.good() && dstFile.good();
    };

    if (!copyFile("/host/etc/passwd", "/etc/passwd"))
    {
        LOG_WARNING("Failed to sync /etc/passwd from /host/etc");
    }
    if (!copyFile("/host/etc/shadow", "/etc/shadow"))
    {
        LOG_WARNING("Failed to sync /etc/shadow from /host/etc");
    }
    if (!copyFile("/host/etc/group", "/etc/group"))
    {
        LOG_WARNING("Failed to sync /etc/group from /host/etc");
    }
}

void UserMgr::executeUserAdd(const char* userName, const char* groups,
                            bool enabled)
{
    // Use --prefix /host to operate on host's /etc files mounted at /host/etc
    // Do not create home directory (-M) as /host/home is not mounted
    // set EXPIRE_DATE to 0 to disable user, PAM takes 0 as expire on
    // 1970-01-01, that's an implementation-defined behavior
    executeCmd("/usr/sbin/useradd", "--prefix", "/host", userName, "-G", groups,
               "-M", "-N", "-s", "/bin/sh", "-e", (enabled ? "" : "1970-01-01"));

    // Sync files after user creation
    syncHostPasswdFiles();
}

void UserMgr::executeUserDelete(const char* userName)
{
    // Use --prefix /host to operate on host's /etc files mounted at /host/etc
    // -r flag won't work for home dirs since /host/home isn't mounted
    executeCmd("/usr/sbin/userdel", "--prefix", "/host", userName);

    // Sync files after user deletion
    syncHostPasswdFiles();
}

std::vector<std::string> UserMgr::getUsersInGroup(const std::string& groupName)
{
    std::vector<std::string> usersInGroup;
    // Should be more than enough to get the pwd structure.
    std::array<char, 4096> buffer{};
    struct group grp;
    struct group* resultPtr = nullptr;

    int status = getgrnam_r(groupName.c_str(), &grp, buffer.data(),
                            buffer.max_size(), &resultPtr);

    if (!status && (&grp == resultPtr))
    {
        for (; *(grp.gr_mem) != NULL; ++(grp.gr_mem))
        {
            usersInGroup.emplace_back(*(grp.gr_mem));
        }
    }
    else
    {
        LOG_ERROR("Group '%s' not found", groupName.c_str());
        // Don't throw error, just return empty userList - fallback
    }
    return usersInGroup;
}

bool UserMgr::isUserEnabled(const std::string& userName)
{
    std::array<char, 4096> buffer{};
    struct spwd spwd;
    struct spwd* resultPtr = nullptr;
    int status = getspnam_r(userName.c_str(), &spwd, buffer.data(),
                            buffer.max_size(), &resultPtr);
    if (!status && (&spwd == resultPtr))
    {
        // according to chage/usermod code -1 means that account does not expire
        // https://github.com/shadow-maint/shadow/blob/7a796897e52293efe9e210ab8da32b7aefe65591/src/chage.c
        if (resultPtr->sp_expire < 0)
        {
            return true;
        }

        // check account expiration date against current date
        if (resultPtr->sp_expire > currentDate())
        {
            return true;
        }

        return false;
    }
    return false; // assume user is disabled for any error.
}

std::vector<std::string> UserMgr::getUserList()
{
    std::vector<std::string> userList;
    struct passwd pw, *pwp = nullptr;
    std::array<char, 1024> buffer;

    sonic::user::File passwd(passwdFileName, "r");
    if ((passwd)() == NULL)
    {
        LOG_ERROR("Error opening %s", passwdFileName);
        throw std::runtime_error("Failed to open passwd file");
    }

    while (true)
    {
        auto r = fgetpwent_r((passwd)(), &pw, buffer.data(), buffer.max_size(),
                             &pwp);
        if ((r != 0) || (pwp == NULL))
        {
            // Any error, break the loop.
            break;
        }
        #ifdef ENABLE_ROOT_USER_MGMT
            // Add all users whose UID >= 1000 and < 65534
            // and special UID 0.
            if ((pwp->pw_uid == 0) ||
                ((pwp->pw_uid >= 1000) && (pwp->pw_uid < 65534)))
        #else
            if ((pwp->pw_uid >= 1000) && (pwp->pw_uid < 65534))
        #endif
        {
            std::string userName(pwp->pw_name);
            userList.emplace_back(userName);
        }
    }
    endpwent();
    return userList;
}

void UserMgr::initUserObjects(void)
{
    std::vector<std::string> userNameList;
    userNameList = getUserList();

    if (!userNameList.empty())
    {
        std::map<std::string, std::vector<std::string>> groupLists;
        // We only track users that are in the |predefinedGroups|
        // The other groups don't contain real BMC users.
        for (const char* grp : predefinedGroups)
        {
            std::vector<std::string> grpUsersList = getUsersInGroup(grp);
            groupLists.emplace(grp, grpUsersList);
        }
        for (auto& grp : privMgr)
        {
            std::vector<std::string> grpUsersList = getUsersInGroup(grp);
            groupLists.emplace(grp, grpUsersList);
        }

        for (auto& user : userNameList)
        {
            std::vector<std::string> userGroups;
            std::string userPriv;
            for (const auto& grp : groupLists)
            {
                std::vector<std::string> tempGrp = grp.second;
                if (std::find(tempGrp.begin(), tempGrp.end(), user) !=
                    tempGrp.end())
                {
                    if (std::find(privMgr.begin(), privMgr.end(), grp.first) !=
                        privMgr.end())
                    {
                        userPriv = grp.first;
                    }
                    else
                    {
                        userGroups.emplace_back(grp.first);
                    }
                }
            }
            // Add user objects to the Users path.
            sdbusplus::message::object_path tempObjPath(usersObjPath);
            tempObjPath /= user;
            std::string objPath(tempObjPath);
            std::sort(userGroups.begin(), userGroups.end());

            usersList.emplace(user, std::make_unique<Users>(
                                        server, objPath, userGroups,
                                        userPriv, isUserEnabled(user), *this));
        }
    }
}

void UserMgr::createUser(std::string userName,
                         std::vector<std::string> groupNames, std::string priv,
                         bool enabled)
{
    // Todo: (Shreyansh Jain)
    // throwForInvalidPrivilege(priv);
    // Todo: (Shreyansh Jain)
    // throwForInvalidGroup(groupNames);

    throwForUserExists(userName);

    // Todo: (Shreyansh Jain) - Is this function needed?
    // throwForUserNameConstraints(userName, groupNames);

    // Todo: (Shreyansh Jain) - Is this function needed?
    // throwForMaxGrpUserCount(groupNames);

    std::string groups = getCSVFromVector(groupNames);

    // treat privilege as a group - This is to avoid using different file to
    // store the same.
    if (!priv.empty())
    {
        if (groups.size() != 0)
        {
            groups += ",";
        }
        groups += priv;
    }
    try
    {
        executeUserAdd(userName.c_str(), groups.c_str(), enabled);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to create user %s: %s", userName.c_str(),
               e.what());
        throw;
    }

    sdbusplus::message::object_path tempObjPath(usersObjPath);
    tempObjPath /= userName;
    std::string objPath(tempObjPath);
    std::sort(groupNames.begin(), groupNames.end());

    usersList.emplace(
        userName, std::make_unique<Users>(server, objPath, groupNames,
                                          priv, enabled, *this));

    // Register with ObjectMapper so bmcweb can find this user
    if (objectMapper_)
    {
        objectMapper_->registerObject(
            objPath,
            {"xyz.openbmc_project.User.Attributes",
             "xyz.openbmc_project.Object.Delete"},
            "xyz.openbmc_project.User.Manager");
    }

    LOG_INFO("User %s created", userName.c_str());
}

void UserMgr::deleteUser(std::string userName)
{
    throwForUserDoesNotExist(userName);

    // Unregister from ObjectMapper before deleting
    if (objectMapper_)
    {
        sdbusplus::message::object_path tempObjPath(usersObjPath);
        tempObjPath /= userName;
        std::string userPath(tempObjPath);
        objectMapper_->unregisterObject(userPath);
    }

    try
    {
        // Clear user fail records
        executeUserClearFailRecords(userName.c_str());

        executeUserDelete(userName.c_str());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to delete user %s: %s", userName.c_str(),
               e.what());
        throw;
    }

    usersList.erase(userName);
    LOG_INFO("User %s deleted", userName.c_str());
}

UserInfoMap UserMgr::getUserInfo(const std::string& userName)
{
    UserInfoMap userInfo;

    // Check if user exists in our list (local user)
    if (!isUserExist(userName))
    {
        LOG_ERROR("GetUserInfo: User %s not found", userName.c_str());
        throw std::runtime_error("User not found");
    }

    const auto& user = usersList[userName];

    userInfo.emplace("UserPrivilege", user->getUserPrivilege());
    userInfo.emplace("UserGroups", user->getUserGroups());
    userInfo.emplace("UserEnabled", user->getUserEnabled());
    userInfo.emplace("UserLockedForFailedAttempt", user->getUserLockedForFailedAttempt());
    userInfo.emplace("UserPasswordExpired", user->getUserPasswordExpired());
    userInfo.emplace("RemoteUser", false);  // Always false for local users

    return userInfo;
}

} // namespace user
} // namespace sonic
