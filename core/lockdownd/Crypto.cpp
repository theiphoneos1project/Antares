#include "Crypto.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

static X509 *MakeCertificateSkeleton(EVP_PKEY *publicKey) {
    X509 *x = X509_new();
    if (!x) {
        return nullptr;
    }

    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), 0);
    X509_gmtime_adj(X509_get_notBefore(x), 0);
    X509_gmtime_adj(X509_get_notAfter(x), 10 * 365 * 24 * 3600L);
    X509_set_pubkey(x, publicKey);

    return x;
}

static void AddSKI(X509 *x) {
    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, x, x, nullptr, nullptr, 0);

    X509_EXTENSION *extension = X509V3_EXT_conf_nid(nullptr, &context, NID_subject_key_identifier, "hash");
    if (extension) {
        X509_add_ext(x, extension, -1);
        X509_EXTENSION_free(extension);
    }
}

EVP_PKEY *Crypto::GenerateRSAKey(void) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
        return nullptr;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    EVP_PKEY *pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

EVP_PKEY *Crypto::ParseDevicePublicKey(const std::vector<uint8_t>& pem) {
    BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) {
        return nullptr;
    }

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    RSA *rsa = PEM_read_bio_RSAPublicKey(bio, nullptr, nullptr, nullptr);
#ifdef _WIN32
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

    BIO_free(bio);

    if (!rsa) {
        return nullptr;
    }

    EVP_PKEY *publicKey = EVP_PKEY_new();
    if (!publicKey) {
        RSA_free(rsa);
        return nullptr;
    }

    if (EVP_PKEY_assign_RSA(publicKey, rsa) != 1) {
        EVP_PKEY_free(publicKey);
        RSA_free(rsa);
        return nullptr;
    }

    return publicKey;
}

X509 *Crypto::MakeRootCertificate(EVP_PKEY *rootKey) {
    X509 *x = MakeCertificateSkeleton(rootKey);
    if (!x) {
        return nullptr;
    }

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, x, x, nullptr, nullptr, 0);

    X509_EXTENSION *extension = X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints, "critical,CA:TRUE");
    if (extension) {
        X509_add_ext(x, extension, -1);
        X509_EXTENSION_free(extension);
    }

    AddSKI(x);
    X509_sign(x, rootKey, EVP_sha1());

    return x;
}

X509 *Crypto::MakeLeafCertificate(EVP_PKEY *leafPubkey, EVP_PKEY *caKey, X509 *caCertificate) {
    X509 *x = MakeCertificateSkeleton(leafPubkey);
    if (!x) {
        return nullptr;
    }

    X509_set_issuer_name(x, X509_get_subject_name(caCertificate));

    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, caCertificate, x, nullptr, nullptr, 0);

    X509_EXTENSION *basicConstraintsExtension = X509V3_EXT_conf_nid(nullptr, &context, NID_basic_constraints, "critical,CA:FALSE");
    if (basicConstraintsExtension) {
        X509_add_ext(x, basicConstraintsExtension, -1);
        X509_EXTENSION_free(basicConstraintsExtension);
    }

    X509V3_set_ctx(&context, x, x, nullptr, nullptr, 0);
    AddSKI(x);

    X509_EXTENSION *keyUsageExtension = X509V3_EXT_conf_nid(nullptr, &context, NID_key_usage, "critical,digitalSignature,keyEncipherment");
    if (keyUsageExtension) {
        X509_add_ext(x, keyUsageExtension, -1);
        X509_EXTENSION_free(keyUsageExtension);
    }

    X509_sign(x, caKey, EVP_sha1());

    return x;
}

std::optional<std::vector<uint8_t>> Crypto::CertificateToDER(X509 *certificate) {
    if (!certificate) {
        return std::nullopt;
    }

    const int length = i2d_X509(certificate, nullptr);
    if (length <= 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> buf(length);
    uint8_t *pointer = buf.data();
    i2d_X509(certificate, &pointer);

    return buf;
}

std::optional<std::vector<uint8_t>> Crypto::PrivateKeyToDER(EVP_PKEY *privateKey) {
    if (!privateKey) {
        return std::nullopt;
    }

    const int length = i2d_PrivateKey(privateKey, nullptr);
    if (length <= 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> buf(length);
    uint8_t *pointer = buf.data();
    i2d_PrivateKey(privateKey, &pointer);

    return buf;
}

std::string Crypto::GenerateUUID(void) {
    uint8_t random[16];
    RAND_bytes(random, sizeof(random));

    random[6] = (random[6] & 0x0f) | 0x40;
    random[8] = (random[8] & 0x3f) | 0x80;

    char out[37];
    snprintf(out, sizeof(out),
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        random[0], random[1], random[2], random[3],
        random[4], random[5],
        random[6], random[7],
        random[8], random[9],
        random[10], random[11], random[12], random[13], random[14], random[15]
    );

    return out;
}
