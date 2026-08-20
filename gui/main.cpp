#include "MainFrame.hpp"
#include <wx/snglinst.h>
#include <memory>

#ifdef __linux__
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
#endif

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
#endif
};

wxIMPLEMENT_APP(App);