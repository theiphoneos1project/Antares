#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <memory>
#include <string>
#include <chrono>
#include <vector>

#include <wx/wx.h>
#include <wx/hyperlink.h>

#include "lockdownd/LockdownDaemonClient.hpp"
#include "device/Device.hpp"

class MainFrame : public wxFrame {
public:
    MainFrame();
private:
    void OnEnterRecovery(wxCommandEvent&);
    void OnExitRecovery(wxCommandEvent&);
    void OnEnterDFU(wxCommandEvent&);
    void OnJailbreak(wxCommandEvent&);
    void OnTimerPoll(wxTimerEvent&);

    void RefreshUI(void);

    wxHyperlinkCtrl *MakeCustomHyperlink(const wxString& name, const wxString& link);
    
    static std::optional<std::string> GetMarketingProductName(std::string_view productType);
    static std::optional<std::vector<uint8_t>> LoadFile(const std::string& path);
private:
    static constexpr uint32_t s_minimumWindowWidth = 362; 
    static constexpr uint32_t s_minimumWindowHeight = 375;

    static constexpr auto RecoveryTimeout = std::chrono::seconds(15);

    std::string m_sessionID;

    wxStaticText *m_statusText = nullptr;
    wxButton *m_jailbreakButton = nullptr;

    wxTimer m_connectionTimer;

    std::unique_ptr<LockdownDaemonClient> m_lockdowndClient;
    std::unique_ptr<Device> m_device;
};

#endif // MAINFRAME_H