#include "MainFrame.hpp"
#include "USBGuards.hpp"
#include <wx/snglinst.h>
#include <memory>

class App : public wxApp {
public:
    bool OnInit() override {
#ifdef _WIN32
        wxTheApp->SetAppearance(wxApp::Appearance::Dark);
#endif
        wxString name = wxString::Format("Antares-%s", wxGetUserId());
        m_checker = std::make_unique<wxSingleInstanceChecker>(name);
        if (m_checker->IsAnotherRunning()) {
            wxLogError("Another instance is already running. Exiting.");
            return false;
        }

#ifdef __linux__
        m_usbmuxdGuard = std::make_unique<USBGuard>();
        if (!m_usbmuxdGuard->DidSuccessfullyMask()) {
            wxMessageBox(
                "Could not stop usbmuxd. If the device is not detected, run:\nsudo systemctl mask --now usbmuxd\nbefore launching Antares.\nRun sudo systemctl unmask --now usbmuxd after finishing your session to allow normal usbmuxd operation.",
                "Warning",
                wxICON_WARNING
            );
        }
#endif

#ifdef _WIN32
        m_usbMutexGuard = std::make_unique<USBMutexGuard>();
        if (!m_usbMutexGuard->Acquire()) {
            wxMessageBox("Another program is using the usbmux v0 protocol to communicate with an iPhone OS 1 device. Please quit the other program and retry.", "Error", wxICON_ERROR);
            return false;
        }
#endif

#ifdef _WIN32
        wxMessageBox("On Windows, you may have to use a tool such as Zadig to rebind your driver for your connected device to libusbK. You will have to do this for both normal mode and recovery mode. If you connect a different device, you will have to run the steps again.", "Information", wxICON_INFORMATION);
#endif

        auto *frame = new MainFrame();
        frame->Show();
        return true;
    }
private:
    std::unique_ptr<wxSingleInstanceChecker> m_checker;
#ifdef __linux__
    std::unique_ptr<USBGuard> m_usbmuxdGuard;
#elif defined(_WIN32)
    std::unique_ptr<USBMutexGuard> m_usbMutexGuard;
#endif
};

wxIMPLEMENT_APP(App);