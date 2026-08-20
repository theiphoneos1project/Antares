#include <iostream>
#include <optional>
#include <fstream>
#include <chrono>
#include <thread>
#include "device/Device.hpp"
#include "lockdownd/LockdownDaemonClient.hpp"

static constexpr auto RecoveryTimeout = std::chrono::seconds(15);

#ifdef __linux__
struct USBGuard {
public:
    USBGuard() {
        long result = system("systemctl mask --now usbmuxd");
        if (result == 0) {
            m_masked = true;
        }
    }

    ~USBGuard() {
        if (m_masked) {
            system("systemctl unmask --now usbmuxd");
        }
    }

    bool DidSuccessfullyMask(void) const { return m_masked; }
private:
    bool m_masked = false;
};
#endif

static std::optional<std::vector<uint8_t>> LoadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> result(size);
    if (!file.read(reinterpret_cast<char *>(result.data()), size)) {
        return std::nullopt;
    }
    return result;
}

static bool EnsureDeviceInRecoveryMode(Device& device) {
    auto mode = device.GetMode();
    if (!mode.has_value()) {
        std::cerr << "[-] Can't find device mode\n";
        return false;
    }

    bool deviceOpened = device.Open();
    
    if (*mode == Device::Mode::Normal) {
        std::cout << "[+] Sending device to recovery mode...\n";

        device.Close();

        LockdownDaemonClient lockdowndClient;
        bool lockdowndOpened = lockdowndClient.Open();
        if (!lockdowndOpened) {
            std::cerr << "[-] Failed to open USB connection to device...\n";
            return false;
        }
    
        std::string sessionError;
        auto sessionID = lockdowndClient.StartPairedSession(sessionError);
        if (!sessionID.has_value()) {
            std::cerr << "[-] Failed to start paired session: " << sessionError << "\n";
            return false;
        }

        bool enterRecoverySuccess = lockdowndClient.EnterRecoveryMode(*sessionID);
        if (enterRecoverySuccess) {
            std::cout << "[+] Successfully sent device into recovery mode!\n";
        } else {
            std::cout << "[-] Error sending device into recovery mode!\n";
        }

        const auto deadline = std::chrono::steady_clock::now() + RecoveryTimeout;
        do {
            deviceOpened = device.Open();
            if (deviceOpened) {
                auto currentMode = device.GetMode();
                if (currentMode.has_value() && *currentMode == Device::Mode::Recovery) {
                    break;
                }
            }
        } while (std::chrono::steady_clock::now() < deadline); 

        if (!deviceOpened) {
            std::cerr << "[-] Failed to open device in recovery mode\n";
            return false;
        }

        mode = device.GetMode();
        if (!mode.has_value()) {
            std::cerr << "[-] Can't find device mode\n";
            return false;
        }
    }

    if (*mode != Device::Mode::Recovery) {
        std::cerr << "[-] Device is not in recovery mode! :(\n";
        return false;
    }
    
    return true;
}

static void PrintUsage(void) {
    std::cout << "\n"
              << "_\\|/_ Antares Jailbreak CLI  \n"
              << "\"/|\\\" iPhone OS 1.0-1.1.5  \n"
              << "By Nightwind and EthanArbuckle  \n\n"
              << "Usage:\n"
              << "  ./antares --jailbreak\n"
              << "  ./antares --hacktivate\n"
              << "Add --verbose as your last argument for verbose boot.\n\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    bool isVerboseBootEnabled = false;
    if (std::string(argv[argc - 1]) == "--verbose") {
        isVerboseBootEnabled = true;
    }

#ifdef __linux__
    auto usbmuxdGuard = std::make_unique<USBGuard>();
    if (!usbmuxdGuard->DidSuccessfullyMask()) {
        std::cerr << "[-] Could not stop usbmuxd. If the device is not detected, run:\nsudo systemctl mask --now usbmuxd\nbefore launching Antares.\nRun sudo systemctl unmask --now usbmuxd after finishing your session to allow normal usbmuxd operation.\n";
        return EXIT_FAILURE;
    }
#endif

    Device device;
    bool deviceOpened = device.Open();
    if (!deviceOpened) {
        std::cerr << "[-] Failed to open device\n";
        return EXIT_FAILURE;
    }
    
    std::string_view command = argv[1];
    if (command == "--jailbreak") {
        bool inRecovery = EnsureDeviceInRecoveryMode(device);
        if (!inRecovery) {
            return EXIT_FAILURE;
        }

        device.SendCommand("setpicture 0\n");
        device.SendCommand("bgcolor 125 125 0\n");

        auto ramdiskData = LoadFile("core/ramdisk/ramdisk.img");
        if (!ramdiskData.has_value()) {
            std::cerr << "[-] Failed to load ramdisk.img\n";
            device.SendCommand("bgcolor 125 0 0\n");
            return EXIT_FAILURE;
        }

        std::cout << "[+] Sending ramdisk...\n";
        if (!device.SendFile(*ramdiskData, 0x09CC2000)) {
            std::cerr << "[-] Failed to send ramdisk!\n";
            device.SendCommand("bgcolor 125 0 0\n");
            return EXIT_FAILURE;
        }

        device.SendCommand("setenv antares_jailbreak \"1\"\n");

        if (isVerboseBootEnabled) {
            device.SendCommand("setenv antares_verbose_boot \"1\"\n");
        } else {
            device.SendCommand("setenv antares_verbose_boot \"0\"\n");
        }

        device.SendCommand("bgcolor 0 125 0\n");

        device.SendCommand("setenv boot-args \"rd=md0 -s -x pmd0=0x09CC2000.0x0133D000\"\n");
        device.SendCommand("saveenv\n");
        device.SendCommand("fsboot\n");
    } else if (command == "--hacktivate") {
        bool inRecovery = EnsureDeviceInRecoveryMode(device);
        if (!inRecovery) {
            return EXIT_FAILURE;
        }

        device.SendCommand("setpicture 0\n");
        device.SendCommand("bgcolor 125 125 0\n");
        
        auto ramdiskData = LoadFile("core/ramdisk/ramdisk.img");
        if (!ramdiskData.has_value()) {
            std::cerr << "[-] Failed to load ramdisk.img\n";
            device.SendCommand("bgcolor 125 0 0\n");
            return EXIT_FAILURE;
        }
        
        if (!device.SendFile(*ramdiskData, 0x09CC2000)) {
            std::cerr << "[-] Failed to send ramdisk!\n";
            device.SendCommand("bgcolor 125 0 0\n");
            return EXIT_FAILURE;
        }

        device.SendCommand("setenv antares_hacktivate \"1\"\n");

        if (isVerboseBootEnabled) {
            device.SendCommand("setenv antares_verbose_boot \"1\"\n");
        } else {
            device.SendCommand("setenv antares_verbose_boot \"0\"\n");
        }

        device.SendCommand("bgcolor 0 125 0\n");

        device.SendCommand("setenv boot-args \"rd=md0 -s -x pmd0=0x09CC2000.0x0133D000\"\n");
        device.SendCommand("saveenv\n");
        device.SendCommand("fsboot\n");
    } else {
        std::cerr << "[-] Unknown command: " << command << "\n";
        PrintUsage();
        return EXIT_FAILURE;
    }
}