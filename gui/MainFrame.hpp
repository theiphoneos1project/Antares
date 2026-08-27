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
    void OnHacktivate(wxCommandEvent&);
    void OnEnterRecovery(wxCommandEvent&);
    void OnExitRecovery(wxCommandEvent&);
    void OnCustomBootCommands(wxCommandEvent&);
    void OnJailbreak(wxCommandEvent&);
    void OnTimerPoll(wxTimerEvent&);

    void RefreshUI(void);

    bool EnsureDeviceInRecoveryMode(void);

    wxHyperlinkCtrl *MakeCustomHyperlink(const wxString& name, const wxString& link);
    
    static std::optional<std::string> GetMarketingProductName(std::string_view productType);
    static std::optional<std::vector<uint8_t>> LoadFile(const std::string& path);
    static std::string GetResourcesDirectory(void);
private:
    static constexpr uint32_t s_minimumWindowWidth = 362; 
    static constexpr uint32_t s_minimumWindowHeight = 500;

    static constexpr auto RecoveryTimeout = std::chrono::seconds(15);

    std::string m_sessionID;

    wxMenuItem *m_hacktivateItem = nullptr;
    wxMenuItem *m_enterRecoveryItem = nullptr;
    wxMenuItem *m_exitRecoveryItem = nullptr;
    wxMenuItem *m_customBootCommandsItem = nullptr;

    wxStaticText *m_statusText = nullptr;
    wxStaticText *m_specialThanksText = nullptr;
    
    wxCheckBox *m_verboseBootCheckbox = nullptr;
    wxButton *m_jailbreakButton = nullptr;

    wxTimer m_connectionTimer;

    std::unique_ptr<LockdownDaemonClient> m_lockdowndClient;
    std::unique_ptr<Device> m_device;
};

#endif // MAINFRAME_H