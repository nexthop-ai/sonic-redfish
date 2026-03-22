///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include <sdbusplus/asio/object_server.hpp>
#include <memory>
#include <string>
#include <filesystem>

namespace sonic::dbus_bridge
{
    class ObjectMapperService;
}

namespace sonic::certs
{

    // Forward declaration
    class CertificateManager;

    /**
     * @brief Represents a single X.509 certificate
     *
     * This class manages an individual certificate installed in the system.
     * It handles:
     * - Writing certificate to filesystem
     * - Creating OpenSSL hash symlinks
     * - Exposing certificate properties via D-Bus
     * - Certificate deletion
     */
    class Certificate
    {
        public:
            /**
             * @brief Construct a new Certificate object
             *
             * @param objServer D-Bus object server
             * @param objectPath D-Bus object path for this certificate
             * @param certId Unique certificate identifier
             * @param pemString Certificate in PEM format
             * @param installDir Directory to install certificate
             * @param manager Reference to parent CertificateManager
             * @param objectMapper Pointer to ObjectMapper for registration (optional)
             * @param serviceName D-Bus service name for ObjectMapper registration
             */
            Certificate(
                std::shared_ptr<sdbusplus::asio::object_server> objServer,
                const std::string& objectPath,
                const std::string& certId,
                const std::string& pemString,
                const std::filesystem::path& installDir,
                CertificateManager& manager,
                sonic::dbus_bridge::ObjectMapperService* objectMapper = nullptr,
                const std::string& serviceName = ""
            );

            /**
             * @brief Destroy the Certificate object
             *
             * Cleans up D-Bus interfaces
             */
            ~Certificate();

            /**
             * @brief Get the certificate ID
             *
             * @return const std::string& Certificate identifier
             */
            const std::string& getCertId() const { return certId_; }

            /**
             * @brief Get the D-Bus object path
             *
             * @return const std::string& Object path
             */
            const std::string& getObjectPath() const { return objectPath_; }

            /**
             * @brief Delete this certificate
             *
             * Removes certificate from filesystem and D-Bus
             */
            void deleteCertificate();

        private:
            /**
             * @brief Write certificate to filesystem
             *
             * @param pemString Certificate in PEM format
             */
            void writeCertificateToFile(const std::string& pemString);

            /**
             * @brief Create OpenSSL hash symlink
             *
             * Creates symlink: <hash>.0 -> <certId>.pem
             * Required for OpenSSL certificate verification
             *
             * @param pemString Certificate in PEM format
             */
            void createHashSymlink(const std::string& pemString);

            /**
             * @brief Setup D-Bus interfaces for this certificate
             *
             * Registers properties and methods on D-Bus
             *
             * @param pemString Certificate in PEM format
             */
            void setupDBusInterfaces(const std::string& pemString);

            // D-Bus object server
            std::shared_ptr<sdbusplus::asio::object_server> objServer_;

            // D-Bus object path
            std::string objectPath_;

            // Unique certificate identifier
            std::string certId_;

            // Installation directory
            std::filesystem::path installDir_;

            // Reference to parent manager
            CertificateManager& manager_;

            // D-Bus interface for certificate properties
            std::shared_ptr<sdbusplus::asio::dbus_interface> certInterface_;

            // Path to certificate file on disk
            std::filesystem::path certFilePath_;

            // Path to hash symlink
            std::filesystem::path symlinkPath_;

            // ObjectMapper for D-Bus object registration
            sonic::dbus_bridge::ObjectMapperService* objectMapper_;

            // D-Bus service name for ObjectMapper registration
            std::string serviceName_;
    };

} // namespace sonic::certs

