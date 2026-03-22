///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include "certificate.hpp"
#include <sdbusplus/asio/object_server.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <string>
#include <map>
#include <filesystem>

namespace sonic::dbus_bridge
{
    class ObjectMapperService;
}

namespace sonic::certs
{

    /**
     * @brief Manages a collection of X.509 certificates
     *
     * This class provides certificate management functionality:
     * - Installing new certificates via D-Bus
     * - Deleting certificates
     * - Restoring certificates from disk on startup
     * - Reloading services when certificates change
     *
     * Implements OpenBMC D-Bus interfaces:
     * - xyz.openbmc_project.Certs.Install
     * - xyz.openbmc_project.Collection.DeleteAll
     */
    class CertificateManager
    {
        public:
            /**
             * @brief Construct a new Certificate Manager
             *
             * @param io ASIO io_context for async operations
             * @param objServer D-Bus object server
             * @param basePath D-Bus object path (e.g., /xyz/openbmc_project/certs/authority/truststore)
             * @param installDir Filesystem directory to install certificates
             * @param serviceToReload Systemd service to reload after certificate changes
             * @param objectMapper Pointer to ObjectMapper for certificate registration (optional)
             * @param serviceName D-Bus service name for ObjectMapper registration
             */
            CertificateManager(
                boost::asio::io_context& io,
                std::shared_ptr<sdbusplus::asio::object_server> objServer,
                const std::string& basePath,
                const std::filesystem::path& installDir,
                const std::string& serviceToReload,
                sonic::dbus_bridge::ObjectMapperService* objectMapper = nullptr,
                const std::string& serviceName = ""
            );

            /**
             * @brief Destroy the Certificate Manager
             */
            ~CertificateManager();

            /**
             * @brief Install a new certificate
             *
             * D-Bus method: Install(s filePath) -> o objectPath
             *
             * @param filePath Path to certificate file (PEM format)
             * @return std::string D-Bus object path of installed certificate
             * @throws std::runtime_error if certificate is invalid or already exists
             */
            std::string install(const std::string& filePath);

            /**
             * @brief Delete all certificates
             *
             * D-Bus method: DeleteAll() -> void
             */
            void deleteAll();

            /**
             * @brief Delete a specific certificate
             *
             * Called by Certificate::deleteCertificate()
             *
             * @param certId Certificate identifier to delete
             */
            void deleteCertificate(const std::string& certId);

        private:
            /**
             * @brief Setup D-Bus interfaces
             *
             * Registers Install and DeleteAll methods on D-Bus
             */
            void setupDBusInterfaces();

            /**
             * @brief Restore certificates from disk
             *
             * Scans installDir and creates Certificate objects for existing certificates
             */
            void restoreCertificates();

            /**
             * @brief Reload the configured systemd service
             *
             * Triggers service reload to pick up new certificates
             */
            void reloadService();

            /**
             * @brief Generate next certificate ID
             *
             * @return uint64_t Next available certificate ID
             */
            uint64_t getNextCertId();

            // ASIO io_context
            boost::asio::io_context& io_;

            // D-Bus object server
            std::shared_ptr<sdbusplus::asio::object_server> objServer_;

            // D-Bus base path
            std::string basePath_;

            // Certificate installation directory
            std::filesystem::path installDir_;

            // Systemd service to reload
            std::string serviceToReload_;

            // D-Bus interface for Install method
            std::shared_ptr<sdbusplus::asio::dbus_interface> installInterface_;

            // D-Bus interface for DeleteAll method
            std::shared_ptr<sdbusplus::asio::dbus_interface> deleteAllInterface_;

            // Collection of installed certificates (certId -> Certificate object)
            std::map<std::string, std::unique_ptr<Certificate>> certificates_;

            // Counter for generating certificate IDs
            uint64_t certIdCounter_;

            // ObjectMapper for D-Bus object registration
            sonic::dbus_bridge::ObjectMapperService* objectMapper_;

            // D-Bus service name for ObjectMapper registration
            std::string serviceName_;
    };

} // namespace sonic::certs

