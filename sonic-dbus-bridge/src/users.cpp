#include "users.hpp"
#include "user_mgr.hpp"
#include "logger.hpp"


namespace sonic
{
namespace user
{

/** @brief Constructs Users object.
 *
 *  @param[in] server  - sdbusplus asio object server
 *  @param[in] path    - D-Bus path
 *  @param[in] groups  - users group list
 *  @param[in] priv    - user privilege
 *  @param[in] enabled - user enabled state
 *  @param[in] parent  - user manager - parent object
 */
Users::Users(sdbusplus::asio::object_server& server, const std::string& path,
             std::vector<std::string> groups, const std::string& priv,
             bool enabled, UserMgr& parent) :
    userName(sdbusplus::message::object_path(path).filename()),
    manager(parent),
    server(server),
    userPrivilege(priv),
    userGroups(std::move(groups)),
    userEnabled(enabled)
{
    // Create D-Bus interface for User.Attributes
    userIface = server.add_interface(path, "xyz.openbmc_project.User.Attributes");

    // Register UserPrivilege property with setter
    userIface->register_property(
        "UserPrivilege", userPrivilege,
        // Setter
        [this](const std::string& newPriv, std::string& oldPriv) {
            if (newPriv == oldPriv)
            {
                return true; // No change needed
            }
            LOG_INFO("Changing privilege for user %s from %s to %s",
                   userName.c_str(), oldPriv.c_str(), newPriv.c_str());

            // Build usermod command to change groups
            // Remove old privilege group and add new one
            std::string groups;
            for (const auto& grp : userGroups)
            {
                if (!groups.empty())
                {
                    groups += ",";
                }
                groups += grp;
            }
            if (!groups.empty())
            {
                groups += ",";
            }
            groups += newPriv;

            // Use --prefix /host to modify host's /etc/group
            // Use the safe executeCmd template from user_mgr.hpp (fork/execv, no shell)
            try
            {
                executeCmd("/usr/sbin/usermod", "--prefix", "/host", "-G",
                           groups.c_str(), userName.c_str());
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Failed to change groups for user %s: %s",
                       userName.c_str(), e.what());
                return false;
            }

            // Sync files from host to container
            manager.syncHostPasswdFiles();

            oldPriv = newPriv;
            userPrivilege = newPriv;
            return true;
        });

    // Register UserGroups property (read-only for now)
    userIface->register_property("UserGroups", userGroups);

    // Register UserEnabled property with setter
    userIface->register_property(
        "UserEnabled", userEnabled,
        // Setter
        [this](const bool& newEnabled, bool& oldEnabled) {
            if (newEnabled == oldEnabled)
            {
                return true; // No change needed
            }
            LOG_INFO("Changing enabled state for user %s from %d to %d",
                   userName.c_str(), oldEnabled, newEnabled);

            // Use --prefix /host to modify host's /etc/shadow
            // Use the safe executeCmd template from user_mgr.hpp (fork/execv, no shell)
            try
            {
                if (newEnabled)
                {
                    // Enable user by removing expiration date
                    executeCmd("/usr/sbin/usermod", "--prefix", "/host", "-e", "",
                               userName.c_str());
                }
                else
                {
                    // Disable user by setting expiration to 1970-01-01
                    executeCmd("/usr/sbin/usermod", "--prefix", "/host", "-e",
                               "1970-01-01", userName.c_str());
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Failed to change enabled state for user %s: %s",
                       userName.c_str(), e.what());
                return false;
            }

            // Sync files from host to container
            manager.syncHostPasswdFiles();

            oldEnabled = newEnabled;
            userEnabled = newEnabled;
            return true;
        });

    userIface->register_property("UserLockedForFailedAttempt", false);
    userIface->register_property("UserPasswordExpired", false);

    userIface->initialize();

    // Create D-Bus interface for Object.Delete
    // This allows bmcweb to call Delete() on individual user objects
    deleteIface = server.add_interface(path, "xyz.openbmc_project.Object.Delete");

    // Capture userName by value since this object may be deleted during callback
    std::string userNameCopy = userName;
    deleteIface->register_method("Delete", [&mgr = manager, userNameCopy]() {
        LOG_INFO("Delete method called for user: %s", userNameCopy.c_str());
        mgr.deleteUser(userNameCopy);
    });

    deleteIface->initialize();

    LOG_INFO("User object created for: %s", userName.c_str());
}

Users::~Users()
{
    LOG_INFO("Removing D-Bus interfaces for user: %s", userName.c_str());

    // Remove interfaces from D-Bus
    server.remove_interface(deleteIface);
    server.remove_interface(userIface);
}

} // namespace user
} // namespace sonic
