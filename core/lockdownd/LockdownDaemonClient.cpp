//
// Copyright (c) 2026 Nightwind
//

#include "LockdownDaemonClient.hpp"
#include "Crypto.hpp"
#include <limits.h>
#include <iostream>
#include <fstream>

LockdownDaemonClient::LockdownDaemonClient() : m_client(std::make_unique<lockdownd_client_t>()) {}

LockdownDaemonClient::~LockdownDaemonClient() {
    Close();
}

bool LockdownDaemonClient::IsOpen(void) {
    if (!m_client || !m_client->usb_handle) {
        return false;
    }

    PList::Dictionary request;
    request.Set("Label", PList::String(LockdownDaemonLabel.data()));
    request.Set("Request", PList::String("QueryType"));

    auto response = ExchangePlist(request);
    if (!response.has_value()) {
        return false;
    }

    auto typeNode = response->Get<PList::String>("Type");
    if (typeNode && typeNode->GetType() == PLIST_STRING && typeNode->GetValue() == "com.apple.mobile.lockdown") {
        return true;
    }

    Close();
    return false;
}

bool LockdownDaemonClient::Open(void) {
    if (IsOpen()) {
        return true;
    }

    Close();

    m_client = std::make_unique<lockdownd_client_t>();

    int initStatus = libusb_init(&m_client->usb_ctx);
    if (initStatus < 0) {
        std::cerr << "Failed to init libusb. Error: " << initStatus << " (" << libusb_strerror(initStatus) << ")\n";
        return false;
    }
    
    #ifdef _WIN32
    libusb_set_option(m_client->usb_ctx, LIBUSB_OPTION_USE_USBDK);
    #endif
    
    int claimStatus = find_and_claim(m_client->usb_ctx, &m_client->usb_handle, &m_client->ep_out, &m_client->ep_in, &m_client->intf_num);
    if (claimStatus < 0) {
        std::cerr << "Failed to claim interface. Error: " << claimStatus << " (" << libusb_strerror(claimStatus) << ")\n";
        return false;
    }
    
    m_client->pipe.handle = m_client->usb_handle;
    m_client->pipe.ep_out = m_client->ep_out;
    m_client->pipe.ep_in = m_client->ep_in;
    m_client->pipe.rx_len = 0;
    
    m_client->session.pipe = &m_client->pipe;
    m_client->session.device_port = LockdownDaemonPort;
    m_client->session.src_port = static_cast<uint16_t>(49152 + rand() % 16384);
    m_client->session.tx_seq = 100;
    m_client->session.rx_ack = 0;
    m_client->session.prebuf_len = 0;

    int connectStatus = session_connect(&m_client->session);
    if (connectStatus < 0) {
        std::cerr << "Failed to connect to session. Error: " << connectStatus << " (" << libusb_strerror(connectStatus) << ")\n";
        return false;
    }

    return true;
}

void LockdownDaemonClient::Close(void) {
    if (m_client) {
        if (m_client->usb_handle) {
            session_close(&m_client->session);

            libusb_release_interface(m_client->usb_handle, m_client->intf_num);
            libusb_close(m_client->usb_handle);
            m_client->usb_handle = nullptr;
        }
        
        if (m_client->usb_ctx) {
            libusb_exit(m_client->usb_ctx);
            m_client->usb_ctx = nullptr;
        }
    }
}

std::optional<std::string> LockdownDaemonClient::StartPairedSession(std::string& outError) {
    auto udid = GetValueString("UniqueDeviceID");
    if (!udid.has_value()) {
        std::cerr << "GetValueString(\"UniqueDeviceID\") failed!\n";
        return std::nullopt;
    }

    char home[PATH_MAX];
    GetHomeDirectory(home, sizeof(home));

    std::string pairingRecordsPath = std::string(home) + "/.ios_pairing_records";
    mkdir(pairingRecordsPath.data(), 0700);

    std::string pairingRecordsPlistPath = pairingRecordsPath + "/" + *udid + ".plist";

    bool needsPair = true;

    auto info = LoadPairingRecordInfo(pairingRecordsPlistPath);
    if (info.has_value()) {
        needsPair = false;
    }

    if (needsPair) {
        const std::string hostIdentifier = Crypto::GenerateUUID();
        info = AttemptPair(pairingRecordsPlistPath, hostIdentifier);
        
        if (!info.has_value()) {
            return std::nullopt;
        }
    }

    auto sessionID = StartSession(info->HostIdentifier, outError);
    if (!sessionID.has_value()) {
        if (!needsPair && outError == "InvalidHostID") {
            std::cerr << "Saved pairing record rejected (InvalidHostID).\n";
            std::cerr << "Deleting " << pairingRecordsPlistPath << " — re-run to re-pair.\n";
            remove(pairingRecordsPlistPath.c_str());
        } else if (!outError.empty()) {
            std::cerr << "StartSession failed: " << outError << "\n";
        } else {
            std::cerr << "StartSession failed\n";
        }

        return std::nullopt;
    }

    return sessionID;
}

std::optional<uint16_t> LockdownDaemonClient::StartService(std::string_view serviceName, std::string& outError) {
    PList::Dictionary request;
    request.Set("Label", PList::String(LockdownDaemonLabel.data()));
    request.Set("Request", PList::String("StartService"));
    request.Set("Service", PList::String(serviceName.data()));

    auto response = ExchangePlist(request);
    if (!response.has_value()) {
        return std::nullopt;
    }

    auto resultNode = response->Get<PList::String>("Result");
    if (!resultNode || resultNode->GetType() != PLIST_STRING) {
        return std::nullopt;
    }

    if (resultNode->GetValue() != "Success") {
        auto errorNode = response->Get<PList::String>("Error");
        if (errorNode && errorNode->GetType() == PLIST_STRING) {
            outError = errorNode->GetValue();
        }
        return std::nullopt;
    }

    auto portNode = response->Get<PList::Integer>("Port");
    if (!portNode || portNode->GetType() != PLIST_UINT) {
        return std::nullopt;
    }

    return static_cast<uint16_t>(portNode->GetValue());
}

bool LockdownDaemonClient::EnterRecoveryMode(std::string_view sessionID) {
    PList::Dictionary request;
    request.Set("Request", PList::String("EnterRecovery"));
    request.Set("HostID", PList::String(sessionID.data()));
    return ExchangePlist(request).has_value();
}

std::optional<std::string> LockdownDaemonClient::GetValueString(std::string_view key) {
    auto response = GetValueResponse(key);
    if (!response.has_value()) {
        return std::nullopt;
    }
    
    auto valueNode = response->Get<PList::String>("Value");
    if (!valueNode || valueNode->GetType() != PLIST_STRING) {
        return std::nullopt;
    }

    return valueNode->GetValue();
}

std::optional<std::vector<uint8_t>> LockdownDaemonClient::GetValueData(std::string_view key) {
    auto response = GetValueResponse(key);
    if (!response.has_value()) {
        return std::nullopt;
    }
    
    auto valueNode = response->Get<PList::Data>("Value");
    if (!valueNode || valueNode->GetType() != PLIST_DATA) {
        return std::nullopt;
    }

    const std::vector<char> value = valueNode->GetValue();
    return std::vector<uint8_t>(value.begin(), value.end());
}

std::optional<PList::Dictionary> LockdownDaemonClient::GetValueResponse(std::string_view key) {
    PList::Dictionary request;
    request.Set("Label", PList::String(LockdownDaemonLabel.data()));
    request.Set("Request", PList::String("GetValue"));
    request.Set("Key", PList::String(key.data()));
    return ExchangePlist(request);
}

std::optional<PairingRecordInfo> LockdownDaemonClient::AttemptPair(std::string_view recordPath, std::string_view hostIdentifier) {
    PairingSSLInfo info;
    info.RootKey.reset(Crypto::GenerateRSAKey());
    info.HostKey.reset(Crypto::GenerateRSAKey());

    if (!info.RootKey || !info.HostKey) {
        std::cerr << "Key generation failed\n";
        return std::nullopt;
    }
    
    auto devicePublicKeyData = GetValueData("DevicePublicKey");
    if (!devicePublicKeyData.has_value()) {
        return std::nullopt;
    }
    
    info.DevicePublicKey.reset(Crypto::ParseDevicePublicKey(*devicePublicKeyData));
    if (!info.DevicePublicKey) {
        std::cerr << "Failed to parse device public key\n";
        return std::nullopt;
    }

    info.RootCertificate.reset(Crypto::MakeRootCertificate(info.RootKey.get()));
    info.HostCertificate.reset(Crypto::MakeLeafCertificate(info.HostKey.get(), info.RootKey.get(), info.RootCertificate.get()));
    info.DeviceCertificate.reset(Crypto::MakeLeafCertificate(info.DevicePublicKey.get(), info.RootKey.get(), info.RootCertificate.get()));
    
    if (!info.RootCertificate || !info.HostCertificate || !info.DeviceCertificate) {
        std::cerr << "Certificate generation failed\n";
        return std::nullopt;
    }

    auto rootCertificateDER = Crypto::CertificateToDER(info.RootCertificate.get());
    auto hostCertificateDER = Crypto::CertificateToDER(info.HostCertificate.get());
    auto deviceCertificateDER = Crypto::CertificateToDER(info.DeviceCertificate.get());
    auto hostKeyDER = Crypto::PrivateKeyToDER(info.HostKey.get());

    if (!rootCertificateDER || !hostCertificateDER || !deviceCertificateDER || !hostKeyDER) {
        std::cerr << "Failed to serialize pairing credentials\n";
        return std::nullopt;
    }

    PList::Dictionary request;
    PList::Dictionary pairRecord;

    request.Set("Request", PList::String("Pair"));

    pairRecord.Set("DeviceCertificate", PList::Data(reinterpret_cast<const char *>(deviceCertificateDER->data()), deviceCertificateDER->size()));
    pairRecord.Set("HostCertificate", PList::Data(reinterpret_cast<const char *>(hostCertificateDER->data()), hostCertificateDER->size()));
    pairRecord.Set("HostID", PList::String(hostIdentifier.data()));
    pairRecord.Set("RootCertificate", PList::Data(reinterpret_cast<const char *>(rootCertificateDER->data()), rootCertificateDER->size()));
    
    request.Set("PairRecord", pairRecord);

    auto response = ExchangePlist(request);
    if (!response) {
        std::cerr << "Pair request failed\n";
        return std::nullopt;
    }

    auto resultNode = response->Get<PList::String>("Result");
    if (!resultNode || resultNode->GetType() != PLIST_STRING) {
        std::cerr << "Pair response missing Result\n";
        return std::nullopt;
    }

    const std::string result = resultNode->GetValue();

    if (result != "Success") {
        auto errorNode = response->Get<PList::String>("Error");
        if (errorNode && errorNode->GetType() == PLIST_STRING) {
            std::cerr << "Pair failed: " << result << " (" << errorNode->GetValue() << ")\n";
        } else {
            std::cerr << "Pair failed: " << result << "\n";
        }

        return std::nullopt;
    }

    if (!SavePairingRecordInfo(recordPath, hostIdentifier, *deviceCertificateDER, *hostCertificateDER, *rootCertificateDER, *hostKeyDER)) {
        std::cerr << "Failed to save pairing record: " << recordPath << "\n";
        return std::nullopt;
    }
    
    PairingRecordInfo pairingInfo;
    pairingInfo.HostIdentifier = std::string(hostIdentifier);
    return pairingInfo;
}

std::optional<std::vector<uint8_t>> LockdownDaemonClient::Exchange(const std::vector<uint8_t>& request) {
    uint8_t header[4];

    w32be(header, static_cast<uint32_t>(request.size()));

    if (session_send_frame(&m_client->session, TCP_ACK, header, sizeof(header)) < 0) {
        return std::nullopt;
    }

    if (session_send_frame(&m_client->session, TCP_ACK, request.data(), static_cast<int>(request.size())) < 0) {
        return std::nullopt;
    }

    if (session_recv(&m_client->session, header, sizeof(header)) < 0) {
        return std::nullopt;
    }

    const uint32_t responseLength = r32be(header);
    if (responseLength == 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> response(responseLength);

    size_t received = 0;
    while (received < response.size()) {
        const int count = session_recv(&m_client->session, response.data() + received, static_cast<int>(response.size() - received));

        if (count <= 0) {
            return std::nullopt;
        }

        received += static_cast<size_t>(count);
    }

    return response;
}

std::optional<PList::Dictionary> LockdownDaemonClient::ExchangePlist(const PList::Dictionary& request) {
    const std::string xmlString = request.ToXml();
    if (xmlString.empty()) {
        return std::nullopt;
    }

    const std::vector<uint8_t> requestData(xmlString.begin(), xmlString.end());
    auto response = Exchange(requestData);
    if (!response.has_value()) {
        return std::nullopt;
    }

    std::unique_ptr<PList::Structure> structure(PList::Structure::FromMemory(reinterpret_cast<const char *>(response->data()), response->size()));
    if (!structure || structure->GetType() != PLIST_DICT) {
        return std::nullopt;
    }

    return PList::Dictionary(*static_cast<PList::Dictionary *>(structure.get()));
}

std::optional<std::string> LockdownDaemonClient::StartSession(std::string_view hostIdentifier, std::string& outError) {
    PList::Dictionary request;
    request.Set("Label", PList::String(LockdownDaemonLabel.data()));
    request.Set("Request", PList::String("StartSession"));
    request.Set("HostID", PList::String(hostIdentifier.data()));

    auto response = ExchangePlist(request);
    if (!response.has_value()) {
        return std::nullopt;
    }

    auto resultNode = response->Get<PList::String>("Result");
    if (!resultNode || resultNode->GetType() != PLIST_STRING) {
        return std::nullopt;
    }

    if (resultNode->GetValue() != "Success") {
        auto errorNode = response->Get<PList::String>("Error");
        if (errorNode && errorNode->GetType() == PLIST_STRING) {
            outError = errorNode->GetValue();
        }
        return std::nullopt;
    }

    auto sessionIDNode = response->Get<PList::String>("SessionID");
    if (!sessionIDNode || sessionIDNode->GetType() != PLIST_STRING) {
        return std::nullopt;
    }

    return sessionIDNode->GetValue();
}

std::optional<PairingRecordInfo> LockdownDaemonClient::LoadPairingRecordInfo(std::string_view recordPath) {
    std::ifstream file(recordPath.data(), std::ios::binary | std::ios::ate);
    if (!file) {
        return std::nullopt;
    }

    const auto size = file.tellg();
    file.seekg(0);

    std::vector<char> data(static_cast<size_t>(size));
    if (!file.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        return std::nullopt;
    }

    std::unique_ptr<PList::Structure> plist(PList::Structure::FromMemory(data));
    if (!plist || plist->GetType() != PLIST_DICT) {
        return std::nullopt;
    }

    auto *dictionary = static_cast<PList::Dictionary *>(plist.get());

    auto node = dictionary->Get<PList::String>("HostID");
    if (!node || node->GetType() != PLIST_STRING) {
        return std::nullopt;
    }

    PairingRecordInfo info;
    info.HostIdentifier = node->GetValue();
    return info;
}

bool LockdownDaemonClient::SavePairingRecordInfo(
    std::string_view path, 
    std::string_view hostIdentifier, 
    const std::vector<uint8_t>& deviceCertificateDER, 
    const std::vector<uint8_t>& hostCertificateDER, 
    const std::vector<uint8_t>& rootCertificateDER, 
    const std::vector<uint8_t>& hostKeyDER
) {
    PList::Dictionary dictionary;
    dictionary.Set("DeviceCertificate", PList::Data(reinterpret_cast<const char *>(deviceCertificateDER.data()), deviceCertificateDER.size()));
    dictionary.Set("HostCertificate", PList::Data(reinterpret_cast<const char *>(hostCertificateDER.data()), hostCertificateDER.size()));
    dictionary.Set("HostID", PList::String(hostIdentifier.data()));
    dictionary.Set("HostPrivateKey", PList::Data(reinterpret_cast<const char *>(hostKeyDER.data()), hostKeyDER.size()));
    dictionary.Set("RootCertificate", PList::Data(reinterpret_cast<const char *>(rootCertificateDER.data()), rootCertificateDER.size()));

    const std::string xmlString = dictionary.ToXml();
    if (xmlString.empty()) {
        return false;
    }

    std::ofstream file(path.data(), std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open pairing record for writing: " << path << "\n";
        return false;
    }

    file.write(xmlString.data(), static_cast<std::streamsize>(xmlString.size()));
    return file.good();
}
