#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include "device/Device.hpp"
#include "lockdownd/LockdownDaemonClient.hpp"

static bool LoadFile(const std::string& path, std::vector<uint8_t>& outData) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    outData.resize(size);
    if (!file.read(reinterpret_cast<char *>(outData.data()), size)) {
        return false;
    }
    return true;
}

static constexpr auto RecoveryTimeout = std::chrono::seconds(15);

int main(void) {
    Device device;
    bool deviceOpened = device.Open();
    if (!deviceOpened) {
        std::cerr << "[-] Failed to open device\n";
        return EXIT_FAILURE;
    }

    auto mode = device.GetMode();
    if (!mode.has_value()) {
        std::cerr << "[-] Can't find device mode\n";
        return EXIT_FAILURE;
    }
    
    if (*mode == Device::Mode::Normal) {
        std::cout << "[+] Sending device to recovery mode...\n";

        LockdownDaemonClient lockdowndClient;
        bool lockdowndOpened = lockdowndClient.Open();
        if (!lockdowndOpened) {
            std::cerr << "[-] Failed to open USB connection to device...\n";
            return EXIT_FAILURE;
        }
    
        std::string sessionError;
        auto sessionID = lockdowndClient.StartPairedSession(sessionError);
        if (!sessionID.has_value()) {
            std::cerr << "[-] Failed to start paired session: " << sessionError << "\n";
            return EXIT_FAILURE;
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
            return EXIT_FAILURE;
        }

        mode = device.GetMode();
        if (!mode.has_value()) {
            std::cerr << "[-] Can't find device mode\n";
            return EXIT_FAILURE;
        }
    }

    if (*mode != Device::Mode::Recovery) {
        std::cerr << "[-] Device is not in recovery mode! :(\n";
        return EXIT_FAILURE;
    }

    std::cout << "Here we go!\n";
    
    std::vector<uint8_t> ramdiskData;
    if (!LoadFile("core/ramdisk/ramdisk.img", ramdiskData)) {
        std::cerr << "[-] Failed to load zibri.dat\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "[+] Sending ramdisk...\n";
    if (!device.SendFile(ramdiskData, 0x09CC2000)) {
        std::cerr << "[-] Failed to send ramdisk!\n";
        return EXIT_FAILURE;
    }

    device.SendCommand("setenv boot-args \"rd=md0 -s -x pmd0=0x09CC2000.0x0133D000\"\n");
    device.SendCommand("saveenv\n");
    device.SendCommand("fsboot\n");
}