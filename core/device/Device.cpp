#include "Device.hpp"
#include <iostream>

Device::Device() {
    int success = libusb_init(&m_context);
    
    #ifdef _WIN32
    libusb_set_option(m_context, LIBUSB_OPTION_USE_USBDK);
    #endif

    if (success != LIBUSB_SUCCESS) {
        m_context = nullptr;
    }
}

bool Device::Open(void) {
    if (m_context == nullptr) {
        return false;
    }

    m_deviceHandle = libusb_open_device_with_vid_pid(m_context, AppleVendorID, static_cast<uint16_t>(PID::Recovery));
    if (m_deviceHandle != NULL) {
        m_mode = Mode::Recovery;
    } else {
        m_deviceHandle = libusb_open_device_with_vid_pid(m_context, AppleVendorID, static_cast<uint16_t>(PID::NormaliPhone));
        if (m_deviceHandle != NULL) {
            m_mode = Mode::Normal;
        } else {
            m_deviceHandle = libusb_open_device_with_vid_pid(m_context, AppleVendorID, static_cast<uint16_t>(PID::NormaliPod));
            if (m_deviceHandle != NULL) {
                m_mode = Mode::Normal;
            }
        }
    }

    if (!m_mode) {
        return false;
    }

    if (libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER)) {
        libusb_set_auto_detach_kernel_driver(m_deviceHandle, 1);
    }
    
    int claimResult = libusb_claim_interface(m_deviceHandle, m_mode == Mode::Normal ? 1 : 0);
    if (claimResult != LIBUSB_SUCCESS) {
        libusb_close(m_deviceHandle);
        m_deviceHandle = nullptr;
        m_mode = std::nullopt;
        return false;
    }

    return true;
}

Device::~Device() {
    if (m_deviceHandle) {
        libusb_release_interface(m_deviceHandle, 0);
        libusb_close(m_deviceHandle);
        m_deviceHandle = nullptr;
    }
    
    if (m_context) {
        libusb_exit(m_context);
        m_context = nullptr;
    }
}

bool Device::InitHandshake(void) {
    iboot_message_t message;
    message.cmdcode = static_cast<int16_t>(Command::Initialize);
    message.constant = 0x1234;
    message.size = 0;
    message.unknown = 0;
    return SendControl(&message).has_value();
}

bool Device::SendCommand(std::string_view command) {
    const size_t paddedLength = (((command.size() - 1) / 0x10) + 1) * 0x10;

    iboot_message_t message;
    message.cmdcode = static_cast<int16_t>(Command::SendCommand);
    message.constant = 0x1234;
    message.size = static_cast<int32_t>(paddedLength);
    message.unknown = 0;

    auto receivedMessage = SendControl(&message);
    if (!receivedMessage.has_value()) {
        std::cerr << "[-] Failed to send command!\n";
        return false;
    }

    if (receivedMessage->cmdcode != static_cast<int16_t>(Command::ACK)) {
        std::cerr << "[-] Command not ACK'ed: got " << receivedMessage->cmdcode << "\n";
        return false;
    }

    std::vector<unsigned char> buffer(paddedLength, 0);
    memcpy(buffer.data(), command.data(), command.size());

    int transferred = 0;
    int result = libusb_interrupt_transfer(m_deviceHandle, static_cast<unsigned char>(Endpoint::CommandOut), buffer.data(), static_cast<int>(paddedLength), &transferred, USBTimeout);
    if (result != LIBUSB_SUCCESS) {
        std::cerr << "[-] Command bulk transfer failed: " << libusb_error_name(result) << "\n";
        return false;
    }
    
    return true;
}

bool Device::SendFile(const std::vector<uint8_t>& data, uint32_t loadAddress) {
    const size_t dataSize = data.size();

    iboot_message_t message;
    message.cmdcode = static_cast<int16_t>(Command::SendFile);
    message.constant = 0x1234;
    message.size = static_cast<int32_t>(dataSize);
    message.unknown = static_cast<int32_t>(loadAddress);

    auto receivedMessage = SendControl(&message);
    if (!receivedMessage.has_value()) {
        std::cerr << "[-] Did not receive a valid response\n";
        return false;
    }

    if (receivedMessage->cmdcode != static_cast<int16_t>(Command::ACK)) {
        std::cerr << "[-] Command not ACK'ed: got 0x" << std::hex << receivedMessage->cmdcode << "\n";
        return false;
    }

    const size_t chunkSize = 0x4000;
    
    size_t sent = 0;
    while (sent < dataSize) {
        size_t toSend = (dataSize - sent > chunkSize) ? chunkSize : (dataSize - sent);
        unsigned char *chunk = const_cast<unsigned char *>(data.data()) + sent;
        
        int transferred = 0;
        int result = libusb_interrupt_transfer(m_deviceHandle, static_cast<unsigned char>(Endpoint::FileOut), chunk, static_cast<int>(toSend), &transferred, USBTimeout);
        if (result != LIBUSB_SUCCESS) {
            std::cerr << "[-] File bulk transfer failed: " << libusb_error_name(result) << "\n";
            return false;
        }

        sent += transferred;
    }
    
    return true;
}

std::optional<iboot_message_t> Device::SendControl(iboot_message_t *message) {
    int result;
    
    int transferred = 0;
    result = libusb_interrupt_transfer(m_deviceHandle, static_cast<unsigned char>(Endpoint::ControlOut), reinterpret_cast<unsigned char *>(message), sizeof(iboot_message_t), &transferred, USBTimeout);
    if (result != LIBUSB_SUCCESS) {
        std::cerr << "[-] Control OUT failed: " << libusb_error_name(result) << "\n";
        return std::nullopt;
    }
    
    iboot_message_t receivedMessage;
    result = libusb_interrupt_transfer(m_deviceHandle, static_cast<unsigned char>(Endpoint::ControlIn), reinterpret_cast<unsigned char *>(&receivedMessage), sizeof(iboot_message_t), &transferred, USBTimeout);
    if (result != LIBUSB_SUCCESS) {
        std::cerr << "[-] Control IN failed: " << libusb_error_name(result) << "\n";
        return std::nullopt;
    }

    return receivedMessage;
}
