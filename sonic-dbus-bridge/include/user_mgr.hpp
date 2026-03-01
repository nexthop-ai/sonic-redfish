#pragma once

#include "users.hpp"
#include "object_mapper.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "syslog.h"

namespace sonic
{
namespace user
{

using UserInfoMap = std::map<std::string, std::variant<std::string, std::vector<std::string>, bool>>;

template <typename... ArgTypes>
std::vector<std::string> executeCmd(const char* path, ArgTypes&&... tArgs)
{
    int pipefd[2];

    if (pipe(pipefd) == -1)
    {
        syslog(LOG_ERR, "Failed to create pipe: %s", strerror(errno));
        throw std::runtime_error("Failed to create pipe");
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        syslog(LOG_ERR, "Failed to fork process: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("Failed to fork process");
    }

    if (pid == 0)  // Child process
    {
        close(pipefd[0]);  // Close read end of pipe

        // Redirect write end of the pipe to stdout
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            syslog(LOG_ERR, "Failed to redirect stdout: %s", strerror(errno));
            _exit(EXIT_FAILURE);
        }
        close(pipefd[1]);  // Close write end of pipe

        std::vector<const char*> args = {path};
        (args.emplace_back(const_cast<const char*>(tArgs)), ...);
        args.emplace_back(nullptr);

        execv(path, const_cast<char* const*>(args.data()));

        // If exec returns, an error occurred
        syslog(LOG_ERR, "Failed to execute command '%s': %s", path, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    // Parent process
    close(pipefd[1]);  // Close write end of pipe

    FILE* fp = fdopen(pipefd[0], "r");
    if (fp == nullptr)
    {
        syslog(LOG_ERR, "Failed to open pipe for reading: %s", strerror(errno));
        close(pipefd[0]);
        throw std::runtime_error("Failed to open pipe for reading");
    }

    std::vector<std::string> results;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != nullptr)
    {
        std::string line = buffer;
        if (!line.empty() && line.back() == '\n')
        {
            line.pop_back();  // Remove newline character
        }
        results.emplace_back(line);
    }

    fclose(fp);
    close(pipefd[0]);

    int status;
    while (waitpid(pid, &status, 0) == -1)
    {
        // Need to retry on EINTR
        if (errno == EINTR)
        {
            continue;
        }

        syslog(LOG_ERR, "Failed to wait for child process: %s", strerror(errno));
        throw std::runtime_error("Failed to wait for child process");
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    {
        syslog(LOG_ERR, "Command %s execution failed, return code %d",
               path, WEXITSTATUS(status));
        throw std::runtime_error("Command execution failed");
    }

    return results;
}

class UserMgr
{
  public:
    /** @brief Constructs UserMgr object.
     *
     *  @param[in] server        - sdbusplus asio object server
     *  @param[in] path          - D-Bus path
     *  @param[in] objectMapper  - ObjectMapper service for registration (optional)
     */
    UserMgr(sdbusplus::asio::object_server& server, const char* path,
            sonic::dbus_bridge::ObjectMapperService* objectMapper = nullptr);

    /** @brief Get reference to user objects map
     *
     *  @return const reference to usersList map
     */
    const std::unordered_map<std::string, std::unique_ptr<Users>>& getUsers() const
    {
        return usersList;
    }

    /** @brief delete user method.
     *  This method deletes the user as requested
     *
     *  @param[in] userName - Name of the user which has to be deleted
     */
    void deleteUser(std::string userName);

    /** @brief get user info
     *  Returns user properties for the given user name
     *
     *  @param[in] userName - Name of the user
     *  @return - map of user properties
     */
    UserInfoMap getUserInfo(const std::string& userName);

    /** @brief Sync passwd/shadow/group from /host/etc to container's /etc */
    void syncHostPasswdFiles();

  private:
    /** @brief sdbusplus asio object server */
    sdbusplus::asio::object_server& server;

    /** @brief object path */
    const std::string path;

    /** @brief ObjectMapper service for user registration */
    sonic::dbus_bridge::ObjectMapperService* objectMapper_;

    /** @brief User.Manager D-Bus interface */
    std::shared_ptr<sdbusplus::asio::dbus_interface> userMgrIface;

    /** @brief privilege manager container */
    const std::vector<std::string> privMgr = {"priv-admin", "priv-operator",
                                              "priv-user"};

    /** @brief all groups that can be assigned to users */
    const std::vector<std::string> allGroups = {"redfish"};

    /** @brief map container to hold users object */
    std::unordered_map<std::string, std::unique_ptr<Users>> usersList;

    /** @brief get users in group
     *  method to get group user list
     *
     *  @param[in] groupName - group name
     *
     *  @return userList  - list of users in the group.
     */
    std::vector<std::string> getUsersInGroup(const std::string& groupName);

    /** @brief get user list
     *  method to get the users list.
     *
     *  @return - vector of User lists
     */
    std::vector<std::string> getUserList(void);

    /** @brief initialize the user manager objects
     *  method to initialize the user manager objects accordingly
     */
    void initUserObjects(void);

    /** @brief check if user is enabled
     *
     *  @param[in] userName - name of the user
     *
     *  @return true if enabled, false otherwise
     */
    bool isUserEnabled(const std::string& userName);

    /** @brief check if user exists
     *
     *  @param[in] userName - name of the user
     *
     *  @return true if user exists, false otherwise
     */
    bool isUserExist(const std::string& userName);

    /** @brief check user exists
     *  method to check whether user exist, and throw if not.
     *
     *  @param[in] userName - name of the user
     */
    void throwForUserDoesNotExist(const std::string& userName);

    /** @brief check user does not exist
     *  method to check whether does not exist, and throw if exists.
     *
     *  @param[in] userName - name of the user
     */
    void throwForUserExists(const std::string& userName);

    /** @brief clear user's failure records
     *  method to clear user fail records and throw if failed.
     *
     *  @param[in] userName - name of the user
     */
    virtual void executeUserClearFailRecords(const char* userName);

    virtual void executeUserAdd(const char* userName, const char* groups,
                                bool enabled);

    virtual void executeUserDelete(const char* userName);

    /** @brief create user method.
     *  This method creates a new user as requested
     *
     *  @param[in] userName - Name of the user which has to be created
     *  @param[in] groupNames - Group names list, to which user has to be added.
     *  @param[in] priv - Privilege of the user.
     *  @param[in] enabled - State of the user enabled / disabled.
     */
    void createUser(std::string userName, std::vector<std::string> groupNames,
                    std::string priv, bool enabled);
};

} // namespace user
} // namespace sonic
