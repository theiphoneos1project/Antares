//
// Copyright (c) 2026 Nightwind
//

#ifndef CRYPTO_H
#define CRYPTO_H

#include <optional>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/x509.h>

namespace Crypto {
    EVP_PKEY *GenerateRSAKey(void);
    EVP_PKEY *ParseDevicePublicKey(const std::vector<uint8_t>& pem);
    
    X509 *MakeRootCertificate(EVP_PKEY *rootKey);
    X509 *MakeLeafCertificate(EVP_PKEY *leafPublicKey, EVP_PKEY *caKey, X509 *caCertificate);

    std::optional<std::vector<uint8_t>> CertificateToDER(X509 *certificate);
    std::optional<std::vector<uint8_t>> PrivateKeyToDER(EVP_PKEY *privateKey);

    std::string GenerateUUID(void);
};

#endif // CRYPTO_H