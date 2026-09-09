//
// Copyright (c) 2026 Nightwind
//

#ifndef USBGUARD_H
#define USBGUARD_H

#if defined(__linux__)
struct USBGuard {
public:
    USBGuard() {
        long result = wxExecute("systemctl mask --now usbmuxd", wxEXEC_SYNC);
        if (result == 0) {
            m_masked = true;
        }
    }

    ~USBGuard() {
        if (m_masked) {
            wxExecute("systemctl unmask --now usbmuxd", wxEXEC_SYNC);
        }
    }

    bool DidSuccessfullyMask(void) const { return m_masked; }
private:
    bool m_masked = false;
};
#elif defined(_WIN32)
#include <windows.h>

class USBMutexGuard {
public:
    bool Acquire() {
        m_handle = CreateMutexA(nullptr, TRUE, "Global\\UsbmuxV0");
        return m_handle && GetLastError() != ERROR_ALREADY_EXISTS;
    }

    ~USBMutexGuard() {
        if (m_handle) {
            ReleaseMutex(m_handle);
            CloseHandle(m_handle);
        }
    }

private:
    HANDLE m_handle = nullptr;
};
#endif

#endif // USBGUARD_H
