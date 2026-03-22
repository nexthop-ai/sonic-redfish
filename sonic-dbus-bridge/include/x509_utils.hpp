///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include <string>
#include <cstdint>
#include <openssl/x509.h>

namespace sonic::certs::x509
{

    /**
     * @brief Parse a PEM-encoded certificate string
     *
     * @param pemString Certificate in PEM format
     * @return X509* Pointer to OpenSSL X509 structure (caller must free with X509_free)
     * @throws std::runtime_error if parsing fails
     */
    X509* parsePEMCertificate(const std::string& pemString);

    /**
     * @brief Validate a certificate
     *
     * Checks:
     * - Certificate is not expired
     * - Certificate is already valid (notBefore <= now)
     * - Certificate format is correct
     *
     * @param cert OpenSSL X509 certificate
     * @throws std::runtime_error if validation fails
     */
    void validateCertificate(X509* cert);

    /**
     * @brief Get certificate subject hash
     *
     * Returns the OpenSSL subject hash used for creating symlinks.
     * Format: 8-character hex string (e.g., "346515ab")
     *
     * @param cert OpenSSL X509 certificate
     * @return std::string Subject hash
     */
    std::string getSubjectHash(X509* cert);

    /**
     * @brief Get certificate subject as string
     *
     * Format: "C=IN, ST=Karnataka, L=Bengaluru, O=Nexthop, OU=BMC, CN=Nexthop BMC CA"
     *
     * @param cert OpenSSL X509 certificate
     * @return std::string Subject DN
     */
    std::string getSubject(X509* cert);

    /**
     * @brief Get certificate issuer as string
     *
     * Format: "C=IN, ST=Karnataka, L=Bengaluru, O=Nexthop, OU=BMC, CN=Nexthop BMC CA"
     *
     * @param cert OpenSSL X509 certificate
     * @return std::string Issuer DN
     */
    std::string getIssuer(X509* cert);

    /**
     * @brief Get certificate validity start time
     *
     * @param cert OpenSSL X509 certificate
     * @return uint64_t Unix timestamp (seconds since epoch)
     */
    uint64_t getNotBefore(X509* cert);

    /**
     * @brief Get certificate validity end time
     *
     * @param cert OpenSSL X509 certificate
     * @return uint64_t Unix timestamp (seconds since epoch)
     */
    uint64_t getNotAfter(X509* cert);

    /**
     * @brief Generate a unique certificate ID
     *
     * Uses SHA256 hash of the certificate DER encoding
     *
     * @param cert OpenSSL X509 certificate
     * @return std::string Unique certificate identifier
     */
    std::string generateCertificateId(X509* cert);

    /**
     * @brief RAII wrapper for OpenSSL X509 certificate
     *
     * Automatically frees the certificate when going out of scope
     */
    class X509Ptr
    {
        public:
            explicit X509Ptr(X509* cert) : cert_(cert) {}
            
            ~X509Ptr()
            {
                if (cert_)
                {
                    X509_free(cert_);
                }
            }

            // Disable copy
            X509Ptr(const X509Ptr&) = delete;
            X509Ptr& operator=(const X509Ptr&) = delete;

            // Enable move
            X509Ptr(X509Ptr&& other) noexcept : cert_(other.cert_)
            {
                other.cert_ = nullptr;
            }

            X509Ptr& operator=(X509Ptr&& other) noexcept
            {
                if (this != &other)
                {
                    if (cert_)
                    {
                        X509_free(cert_);
                    }
                    cert_ = other.cert_;
                    other.cert_ = nullptr;
                }
                return *this;
            }

            X509* get() const { return cert_; }
            X509* release()
            {
                X509* tmp = cert_;
                cert_ = nullptr;
                return tmp;
            }

        private:
            X509* cert_;
    };

} // namespace sonic::certs::x509

