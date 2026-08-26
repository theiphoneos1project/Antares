#ifndef DEVICE_H
#define DEVICE_H

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <memory>

#include <libusb-1.0/libusb.h>

#pragma pack(push, 1)
typedef struct {
    int16_t cmdcode;
    int16_t constant;
    int32_t size;
    int32_t unknown;
} iboot_message_t;
#pragma pack(pop)

class Device {
public:
    static constexpr uint32_t USBTimeout = 5000 * 2;
    static constexpr uint16_t AppleVendorID = 0x5AC;

    enum class PID : uint16_t {
        Recovery = 0x1280,
        NormaliPhone = 0x1290,
        NormaliPod = 0x1291
    };

    enum class Mode : uint8_t {
        Normal = 0,
        Recovery = 1
    };

    enum class Endpoint : unsigned char {
        ControlOut = 0x04,
        ControlIn = 0x83,
        CommandOut = 0x02,
        FileOut = 0x05
    };

    enum class Command : int16_t {
        Initialize = 0x0000,
        SendCommand = 0x0803,
        SendFile = 0x0805,
        ACK = 0x0808
    };
public:
    Device();
    ~Device();

    bool Open(void);
    void Close(void);

    bool IsSupported(void);

    std::optional<Mode> GetMode(void) const { return m_mode; }

    bool InitHandshake(void);
    bool SendCommand(std::string_view command);
    bool SendFile(const std::vector<uint8_t>& data, uint32_t loadAddress);

    std::optional<std::string> RecoveryModeGetProductType(void);
private:
    std::optional<iboot_message_t> SendControl(iboot_message_t *message);
private:
    libusb_context *m_context = nullptr;
    libusb_device_handle *m_deviceHandle = nullptr;
    std::optional<Mode> m_mode;
};

#endif // DEVICE_H