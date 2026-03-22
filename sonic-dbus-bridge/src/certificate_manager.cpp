///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#include "certificate_manager.hpp"
#include "x509_utils.hpp"
#include "logger.hpp"
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace sonic::certs
{

    CertificateManager::CertificateManager(
        boost::asio::io_context& io,
        std::shared_ptr<sdbusplus::asio::object_server> objServer,
        const std::string& basePath,
        const std::filesystem::path& installDir,
        const std::string& serviceToReload,
        sonic::dbus_bridge::ObjectMapperService* objectMapper,
        const std::string& serviceName)
        : io_(io),
          objServer_(objServer),
          basePath_(basePath),
          installDir_(installDir),
          serviceToReload_(serviceToReload),
          certIdCounter_(1),
          objectMapper_(objectMapper),
          serviceName_(serviceName)
    {
        LOG_ERROR("Initializing CertificateManager at: %s", basePath_.c_str());

        // Create install directory if it doesn't exist
        if (!std::filesystem::exists(installDir_))
        {
            std::filesystem::create_directories(installDir_);
            LOG_ERROR("Created certificate directory: %s", installDir_.string().c_str());
        }

        // Setup D-Bus interfaces
        setupDBusInterfaces();

        // Restore existing certificates from disk
        restoreCertificates();

        LOG_ERROR("CertificateManager initialized successfully");
    }

    CertificateManager::~CertificateManager()
    {
        LOG_INFO("Destroying CertificateManager");
    }

    std::string CertificateManager::install(const std::string& filePath)
    {
        LOG_ERROR("Installing certificate from: %s", filePath.c_str());

        // Read certificate file
        std::ifstream certFile(filePath);
        if (!certFile)
        {
            throw std::runtime_error("Failed to open certificate file: " + filePath);
        }

        std::string pemString((std::istreambuf_iterator<char>(certFile)),
                              std::istreambuf_iterator<char>());
        certFile.close();

        if (pemString.empty())
        {
            throw std::runtime_error("Certificate file is empty: " + filePath);
        }

        // Parse and validate certificate
        x509::X509Ptr cert(x509::parsePEMCertificate(pemString));
        x509::validateCertificate(cert.get());

        // Generate unique certificate ID
        std::string certId = x509::generateCertificateId(cert.get());

        // Check if certificate already exists
        if (certificates_.find(certId) != certificates_.end())
        {
            throw std::runtime_error("Certificate already exists: " + certId);
        }

        // Create D-Bus object path for this certificate
        std::string objectPath = basePath_ + "/" + std::to_string(getNextCertId());

        LOG_ERROR("Creating certificate with ID: %s at path: %s", certId.c_str(), objectPath.c_str());

        // Create Certificate object
        auto certObj = std::make_unique<Certificate>(
            objServer_,
            objectPath,
            certId,
            pemString,
            installDir_,
            *this,
            objectMapper_,
            serviceName_
        );

        // Add to collection
        certificates_[certId] = std::move(certObj);

        LOG_ERROR("Certificate installed successfully, total certificates: %zu",
                 certificates_.size());

        // Reload service to pick up new certificate
        reloadService();

        // Return D-Bus object path
        return objectPath;
    }

    void CertificateManager::deleteAll()
    {
        LOG_ERROR("Deleting all certificates, count: %zu", certificates_.size());

        // Clear all certificates (destructors will clean up files and D-Bus objects)
        certificates_.clear();

        LOG_ERROR("All certificates deleted");

        // Reload service
        reloadService();
    }

    void CertificateManager::deleteCertificate(const std::string& certId)
    {
        LOG_ERROR("Deleting certificate: %s", certId.c_str());

        auto it = certificates_.find(certId);
        if (it == certificates_.end())
        {
            LOG_ERROR("Certificate not found: %s", certId.c_str());
            return;
        }

        // Remove from collection (destructor will clean up)
        certificates_.erase(it);

        LOG_ERROR("Certificate deleted, remaining: %zu", certificates_.size());

        // Reload service
        reloadService();
    }

    void CertificateManager::setupDBusInterfaces()
    {
        LOG_INFO("Setting up D-Bus interfaces");

        // Create Install interface
        installInterface_ = objServer_->add_interface(
            basePath_,
            "xyz.openbmc_project.Certs.Install"
        );

        // Register Install method
        installInterface_->register_method(
            "Install",
            [this](const std::string& filePath) -> std::string {
                try {
                    fprintf(stderr, "=== D-Bus Install called: %s ===\n", filePath.c_str());
                    fflush(stderr);
                    LOG_ERROR("D-Bus Install method called with: %s", filePath.c_str());
                    std::string result = this->install(filePath);
                    fprintf(stderr, "=== D-Bus Install succeeded: %s ===\n", result.c_str());
                    fflush(stderr);
                    LOG_ERROR("D-Bus Install method succeeded, returning: %s", result.c_str());
                    return result;
                } catch (const std::exception& e) {
                    fprintf(stderr, "=== D-Bus Install FAILED: %s ===\n", e.what());
                    fflush(stderr);
                    LOG_ERROR("D-Bus Install method failed: %s", e.what());
                    throw;
                }
            }
        );

        installInterface_->initialize();

        // Create DeleteAll interface
        deleteAllInterface_ = objServer_->add_interface(
            basePath_,
            "xyz.openbmc_project.Collection.DeleteAll"
        );

        // Register DeleteAll method
        deleteAllInterface_->register_method(
            "DeleteAll",
            [this]() {
                this->deleteAll();
            }
        );

        deleteAllInterface_->initialize();

        LOG_INFO("D-Bus interfaces created successfully");
    }

    void CertificateManager::restoreCertificates()
    {
        LOG_ERROR("Restoring certificates from: %s", installDir_.string().c_str());

        if (!std::filesystem::exists(installDir_))
        {
            LOG_ERROR("Install directory does not exist, no certificates to restore");
            return;
        }

        // Scan directory for .pem files
        for (const auto& entry : std::filesystem::directory_iterator(installDir_))
        {
            if (entry.path().extension() == ".pem")
            {
                try
                {
                    // Read certificate file
                    std::ifstream certFile(entry.path());
                    std::string pemString((std::istreambuf_iterator<char>(certFile)),
                                          std::istreambuf_iterator<char>());
                    certFile.close();

                    // Parse to get certificate ID
                    x509::X509Ptr cert(x509::parsePEMCertificate(pemString));
                    std::string certId = x509::generateCertificateId(cert.get());

                    // Create D-Bus object path
                    std::string objectPath = basePath_ + "/" + std::to_string(getNextCertId());

                    LOG_ERROR("Restoring certificate: %s", certId.c_str());

                    // Create Certificate object
                    auto certObj = std::make_unique<Certificate>(
                        objServer_,
                        objectPath,
                        certId,
                        pemString,
                        installDir_,
                        *this,
                        objectMapper_,
                        serviceName_
                    );

                    certificates_[certId] = std::move(certObj);
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Failed to restore certificate from %s: %s",
                              entry.path().string().c_str(), e.what());
                }
            }
        }

        LOG_ERROR("Restored %zu certificates", certificates_.size());
    }

    void CertificateManager::reloadService()
    {
        if (serviceToReload_.empty())
        {
            LOG_ERROR("No service to reload");
            return;
        }

        LOG_ERROR("Reloading service: %s", serviceToReload_.c_str());

        // TODO: Implement systemd service reload via D-Bus
        // For now, just log the action
        // In production, this would call systemd D-Bus API to reload bmcweb.service

        LOG_INFO("Service reload requested (not yet implemented)");
    }

    uint64_t CertificateManager::getNextCertId()
    {
        return certIdCounter_++;
    }

} // namespace sonic::certs

