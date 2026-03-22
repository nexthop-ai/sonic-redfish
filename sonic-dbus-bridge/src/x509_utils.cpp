///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#include "x509_utils.hpp"
#include "logger.hpp"
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace sonic::certs::x509
{

    X509* parsePEMCertificate(const std::string& pemString)
    {
        // Create BIO from PEM string
        BIO* bio = BIO_new_mem_buf(pemString.data(), pemString.size());
        if (!bio)
        {
            throw std::runtime_error("Failed to create BIO from PEM string");
        }

        // Parse PEM certificate
        X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!cert)
        {
            throw std::runtime_error("Failed to parse PEM certificate");
        }

        return cert;
    }

    void validateCertificate(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        // Get current time
        time_t now = time(nullptr);

        // Check notBefore
        ASN1_TIME* notBefore = X509_get_notBefore(cert);
        if (X509_cmp_time(notBefore, &now) > 0)
        {
            throw std::runtime_error("Certificate is not yet valid");
        }

        // Check notAfter
        ASN1_TIME* notAfter = X509_get_notAfter(cert);
        if (X509_cmp_time(notAfter, &now) < 0)
        {
            throw std::runtime_error("Certificate has expired");
        }

        LOG_INFO("Certificate validation successful");
    }

    std::string getSubjectHash(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        // Get subject hash (used for OpenSSL symlink)
        unsigned long hash = X509_subject_name_hash(cert);

        // Convert to 8-character hex string
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << hash;
        return oss.str();
    }

    std::string getSubject(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        X509_NAME* subject = X509_get_subject_name(cert);
        if (!subject)
        {
            throw std::runtime_error("Failed to get certificate subject");
        }

        // Convert to string
        BIO* bio = BIO_new(BIO_s_mem());
        X509_NAME_print_ex(bio, subject, 0, XN_FLAG_RFC2253);
        
        char* data = nullptr;
        long len = BIO_get_mem_data(bio, &data);
        std::string result(data, len);
        BIO_free(bio);

        return result;
    }

    std::string getIssuer(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        X509_NAME* issuer = X509_get_issuer_name(cert);
        if (!issuer)
        {
            throw std::runtime_error("Failed to get certificate issuer");
        }

        // Convert to string
        BIO* bio = BIO_new(BIO_s_mem());
        X509_NAME_print_ex(bio, issuer, 0, XN_FLAG_RFC2253);
        
        char* data = nullptr;
        long len = BIO_get_mem_data(bio, &data);
        std::string result(data, len);
        BIO_free(bio);

        return result;
    }

    uint64_t getNotBefore(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        ASN1_TIME* notBefore = X509_get_notBefore(cert);
        if (!notBefore)
        {
            throw std::runtime_error("Failed to get notBefore time");
        }

        // Convert ASN1_TIME to time_t
        struct tm tm;
        ASN1_TIME_to_tm(notBefore, &tm);
        return static_cast<uint64_t>(mktime(&tm));
    }

    uint64_t getNotAfter(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        ASN1_TIME* notAfter = X509_get_notAfter(cert);
        if (!notAfter)
        {
            throw std::runtime_error("Failed to get notAfter time");
        }

        // Convert ASN1_TIME to time_t
        struct tm tm;
        ASN1_TIME_to_tm(notAfter, &tm);
        return static_cast<uint64_t>(mktime(&tm));
    }

    std::string generateCertificateId(X509* cert)
    {
        if (!cert)
        {
            throw std::runtime_error("Certificate is null");
        }

        // Get DER encoding of certificate
        unsigned char* derBuf = nullptr;
        int derLen = i2d_X509(cert, &derBuf);
        if (derLen < 0)
        {
            throw std::runtime_error("Failed to encode certificate to DER");
        }

        // Calculate SHA256 hash
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(derBuf, derLen, hash);
        OPENSSL_free(derBuf);

        // Convert to hex string (first 16 bytes for shorter ID)
        std::ostringstream oss;
        for (int i = 0; i < 16; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(hash[i]);
        }

        return oss.str();
    }

} // namespace sonic::certs::x509

