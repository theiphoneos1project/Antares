#ifndef LOCKDOWNDAEMONCLIENT_H
#define LOCKDOWNDAEMONCLIENT_H

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include <plist/plist++.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "usb/mux.h"

typedef struct {
    libusb_context *usb_ctx;
    libusb_device_handle *usb_handle;
    uint8_t ep_out;
    uint8_t ep_in;
    int intf_num;
    usb_pipe_t pipe;
    mux_session_t session;
} lockdownd_client_t;

struct PairingRecordInfo {
    std::string HostIdentifier;
};

struct PairingSSLInfo {
    using PKeyPointer = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using X509Pointer = std::unique_ptr<X509, decltype(&X509_free)>;

    PKeyPointer RootKey{nullptr, &EVP_PKEY_free};
    PKeyPointer HostKey{nullptr, &EVP_PKEY_free};
    PKeyPointer DevicePublicKey{nullptr, &EVP_PKEY_free};
    
    X509Pointer RootCertificate{nullptr, &X509_free};
    X509Pointer HostCertificate{nullptr, &X509_free};
    X509Pointer DeviceCertificate{nullptr, &X509_free};
};

class LockdownDaemonClient {
public:
    static constexpr std::string_view LockdownDaemonLabel = "lockdownd-client";
    static constexpr uint16_t LockdownDaemonPort = 62078;
public:
    LockdownDaemonClient();
    ~LockdownDaemonClient();

    LockdownDaemonClient(const LockdownDaemonClient& rhs) = delete;
    LockdownDaemonClient& operator=(const LockdownDaemonClient& rhs) = delete;

    bool Open(void);
    void Close(void);

    bool IsOpen(void);

    std::optional<std::string> StartPairedSession(std::string& outError);
    std::optional<uint16_t> StartService(std::string_view serviceName, std::string& outError);
    bool EnterRecoveryMode(std::string_view sessionID);

    std::optional<std::string> GetValueString(std::string_view key);
    std::optional<std::vector<uint8_t>> GetValueData(std::string_view key);
    
    usb_pipe_t *GetPipe(void) const noexcept { return &m_client->pipe; };
private:
    std::optional<PList::Dictionary> GetValueResponse(std::string_view key);

    std::optional<PairingRecordInfo> AttemptPair(std::string_view recordPath, std::string_view hostIdentifier);

    std::optional<std::vector<uint8_t>> Exchange(const std::vector<uint8_t>& request);
    std::optional<PList::Dictionary> ExchangePlist(const PList::Dictionary& request);
    
    std::optional<std::string> StartSession(std::string_view hostIdentifier, std::string& outError);

    std::optional<PairingRecordInfo> LoadPairingRecordInfo(std::string_view path);
    bool SavePairingRecordInfo(
        std::string_view path, 
        std::string_view hostIdentifier, 
        const std::vector<uint8_t>& deviceCertificateDER, 
        const std::vector<uint8_t>& hostCertificateDER, 
        const std::vector<uint8_t>& rootCertificateDER, 
        const std::vector<uint8_t>& hostKeyDER
    );
private:
    std::unique_ptr<lockdownd_client_t> m_client;
};

#endif // LOCKDOWNDAEMONCLIENT_H