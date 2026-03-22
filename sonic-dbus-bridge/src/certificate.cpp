///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#include "certificate.hpp"
#include "certificate_manager.hpp"
#include "x509_utils.hpp"
#include "logger.hpp"
#include "object_mapper.hpp"
#include <fstream>
#include <stdexcept>

namespace sonic::certs
{

    Certificate::Certificate(
        std::shared_ptr<sdbusplus::asio::object_server> objServer,
        const std::string& objectPath,
        const std::string& certId,
        const std::string& pemString,
        const std::filesystem::path& installDir,
        CertificateManager& manager,
        sonic::dbus_bridge::ObjectMapperService* objectMapper,
        const std::string& serviceName)
        : objServer_(objServer),
          objectPath_(objectPath),
          certId_(certId),
          installDir_(installDir),
          manager_(manager),
          objectMapper_(objectMapper),
          serviceName_(serviceName)
    {
        LOG_ERROR("Creating certificate object: %s", objectPath.c_str());

        // Write certificate to filesystem
        writeCertificateToFile(pemString);

        // Create OpenSSL hash symlink
        createHashSymlink(pemString);

        // Setup D-Bus interfaces
        setupDBusInterfaces(pemString);

        // Register with ObjectMapper if available
        if (objectMapper_ && !serviceName_.empty())
        {
            LOG_ERROR("Registering certificate with ObjectMapper: %s", objectPath_.c_str());
            objectMapper_->registerObject(
                objectPath_,
                {"xyz.openbmc_project.Certs.Certificate"},
                serviceName_
            );
        }

        LOG_ERROR("Certificate created successfully: %s", certId_.c_str());
    }

    Certificate::~Certificate()
    {
        LOG_ERROR("Destroying certificate: %s", certId_.c_str());
    }

    void Certificate::deleteCertificate()
    {
        LOG_ERROR("Deleting certificate: %s", certId_.c_str());

        // Remove certificate file
        if (std::filesystem::exists(certFilePath_))
        {
            std::filesystem::remove(certFilePath_);
            LOG_ERROR("Removed certificate file: %s", certFilePath_.string().c_str());
        }

        // Remove symlink
        if (std::filesystem::exists(symlinkPath_))
        {
            std::filesystem::remove(symlinkPath_);
            LOG_ERROR("Removed symlink: %s", symlinkPath_.string().c_str());
        }

        // Notify manager to remove this certificate from collection
        manager_.deleteCertificate(certId_);
    }

    void Certificate::writeCertificateToFile(const std::string& pemString)
    {
        // Certificate file path: <installDir>/<certId>.pem
        certFilePath_ = installDir_ / (certId_ + ".pem");

        LOG_ERROR("Writing certificate to: %s", certFilePath_.string().c_str());

        // Write PEM string to file
        std::ofstream certFile(certFilePath_);
        if (!certFile)
        {
            throw std::runtime_error("Failed to open certificate file for writing: " + 
                                     certFilePath_.string());
        }

        certFile << pemString;
        certFile.close();

        if (!certFile)
        {
            throw std::runtime_error("Failed to write certificate to file: " + 
                                     certFilePath_.string());
        }

        LOG_INFO("Certificate written successfully");
    }

    void Certificate::createHashSymlink(const std::string& pemString)
    {
        // Parse certificate to get hash
        x509::X509Ptr cert(x509::parsePEMCertificate(pemString));
        std::string hash = x509::getSubjectHash(cert.get());

        // Symlink path: <installDir>/<hash>.0
        symlinkPath_ = installDir_ / (hash + ".0");

        LOG_ERROR("Creating symlink: %s -> %s.pem", symlinkPath_.string().c_str(), certId_.c_str());

        // Create relative symlink: <hash>.0 -> <certId>.pem
        std::filesystem::path target = certId_ + ".pem";

        // Remove existing symlink if present
        if (std::filesystem::exists(symlinkPath_))
        {
            std::filesystem::remove(symlinkPath_);
            LOG_INFO("Removed existing symlink");
        }

        // Create symlink
        std::filesystem::create_symlink(target, symlinkPath_);

        LOG_INFO("Symlink created successfully");
    }

    void Certificate::setupDBusInterfaces(const std::string& pemString)
    {
        // Parse certificate to extract properties
        x509::X509Ptr cert(x509::parsePEMCertificate(pemString));

        std::string subject = x509::getSubject(cert.get());
        std::string issuer = x509::getIssuer(cert.get());
        uint64_t notBefore = x509::getNotBefore(cert.get());
        uint64_t notAfter = x509::getNotAfter(cert.get());

        LOG_ERROR("Setting up D-Bus interface at: %s", objectPath_.c_str());

        // Create D-Bus interface for this certificate
        certInterface_ = objServer_->add_interface(
            objectPath_,
            "xyz.openbmc_project.Certs.Certificate"
        );

        // Register properties (read-only)
        certInterface_->register_property("CertificateString", pemString);
        certInterface_->register_property("Subject", subject);
        certInterface_->register_property("Issuer", issuer);
        certInterface_->register_property("ValidNotBefore", notBefore);
        certInterface_->register_property("ValidNotAfter", notAfter);

        // Register Delete method
        certInterface_->register_method("Delete", [this]() {
            this->deleteCertificate();
        });

        // Initialize the interface
        certInterface_->initialize();

        LOG_INFO("D-Bus interface created successfully");
    }

} // namespace sonic::certs

