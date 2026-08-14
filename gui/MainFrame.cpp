#include "MainFrame.hpp"

#include <fstream>

#include <wx/statline.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/artprov.h>
#include <wx/textdlg.h>

enum {
    ID_TOOL_ENTER_RECOVERY = 1001,
    ID_TOOL_EXIT_RECOVERY,
    ID_TOOL_CUSTOM_BOOT_COMMANDS,
    ID_JAILBREAK,
    ID_VERBOSE_BOOT,
    ID_CONNECTION_TIMER
};

MainFrame::MainFrame() : 
    wxFrame(
        nullptr, 
        wxID_ANY, 
        "Antares Jailbreak", 
        wxDefaultPosition, 
        wxSize(s_minimumWindowWidth, s_minimumWindowHeight), 
        wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX)
    )
{
    auto *menuBar = new wxMenuBar();
    auto *toolsMenu = new wxMenu();
    
    m_enterRecoveryItem = new wxMenuItem(toolsMenu, ID_TOOL_ENTER_RECOVERY, "&Enter Recovery");
    toolsMenu->Append(m_enterRecoveryItem);
    
    m_exitRecoveryItem = new wxMenuItem(toolsMenu, ID_TOOL_EXIT_RECOVERY, "&Exit Recovery");
    toolsMenu->Append(m_exitRecoveryItem);
    
    toolsMenu->AppendSeparator();

    m_customBootCommandsItem = new wxMenuItem(toolsMenu, ID_TOOL_CUSTOM_BOOT_COMMANDS, "&Send custom boot commands");
    toolsMenu->Append(m_customBootCommandsItem);

    menuBar->Append(toolsMenu, "&Tools");
    SetMenuBar(menuBar);

    auto *root = new wxBoxSizer(wxVERTICAL);

    auto titleFont = GetFont();
    titleFont.SetPointSize(20);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);

    auto subtitleFont = GetFont();
    subtitleFont.SetPointSize(15);

    auto *title = new wxStaticText(this, wxID_ANY, "Antares");
    title->SetFont(titleFont);
    root->Add(title, 0, wxLEFT | wxTOP, 15);

    auto *subtitle = new wxStaticText(this, wxID_ANY, "Jailbreak for iPhone OS 1.0 - 1.1.5");
    subtitle->SetFont(subtitleFont);
    root->Add(subtitle, 0, wxLEFT | wxRIGHT, 15);

    auto *credits = new wxStaticText(this, wxID_ANY, "By NightwindDev and EthanArbuckle");
    root->Add(credits, 0, wxLEFT | wxRIGHT, 15);

    root->AddStretchSpacer(1);

    m_statusText = new wxStaticText(this, wxID_ANY, "No device connected.", wxDefaultPosition, wxDefaultSize);
    root->Add(m_statusText, 0, wxLEFT | wxRIGHT, 15);

    root->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL), 0, wxEXPAND | wxALL, 15);

    auto *creditText = new wxStaticText(this, wxID_ANY, "Special thanks to: EthanArbuckle, Zibri (ZiPhone)", wxDefaultPosition, wxDefaultSize);
    root->Add(creditText, 0, wxLEFT, 15);

    root->AddStretchSpacer(1);
    
    root->Add(new wxStaticText(this, wxID_ANY, "References:", wxDefaultPosition, wxDefaultSize), 0, wxLEFT, 15);
    root->Add(MakeCustomHyperlink("EthanArbuckle/iOS1.0-Jailbreak", "https://github.com/EthanArbuckle/iOS1.0-Jailbreak"), 0, wxLEFT, 15);
    root->Add(MakeCustomHyperlink("Zibri/ZiPhone", "https://github.com/Zibri/ZiPhone"), 0, wxLEFT, 15);
    
    root->AddStretchSpacer(1);
    
    root->Add(new wxStaticText(this, wxID_ANY, "Find us at:", wxDefaultPosition, wxDefaultSize), 0, wxLEFT, 15);
    root->Add(MakeCustomHyperlink("The iPhone OS 1 Project GitHub", "https://github.com/theiphoneos1project"), 0, wxLEFT, 15);
    root->Add(MakeCustomHyperlink("Antares GitHub Repository", "https://github.com/theiphoneos1project/Antares"), 0, wxLEFT, 15);
    
    root->AddStretchSpacer(3);
    
    root->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL), 0, wxEXPAND | wxALL, 15);
    
    auto *buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_verboseBootCheckbox = new wxCheckBox(this, ID_VERBOSE_BOOT, "Verbose Boot");
    buttonSizer->Add(m_verboseBootCheckbox, 0, wxCENTER);

    buttonSizer->AddSpacer(15);

    m_jailbreakButton = new wxButton(this, ID_JAILBREAK, "Jailbreak");
    buttonSizer->Add(m_jailbreakButton, 1, wxEXPAND | wxCENTER);

    root->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 15);

    m_connectionTimer.SetOwner(this, ID_CONNECTION_TIMER);
    m_connectionTimer.Start(1000);

    Bind(wxEVT_MENU, &MainFrame::OnEnterRecovery, this, ID_TOOL_ENTER_RECOVERY);
    Bind(wxEVT_MENU, &MainFrame::OnExitRecovery, this, ID_TOOL_EXIT_RECOVERY);
    Bind(wxEVT_MENU, &MainFrame::OnCustomBootCommands, this, ID_TOOL_CUSTOM_BOOT_COMMANDS);
    Bind(wxEVT_BUTTON, &MainFrame::OnJailbreak, this, ID_JAILBREAK);
    Bind(wxEVT_TIMER, &MainFrame::OnTimerPoll, this, ID_CONNECTION_TIMER);

    SetSizer(root);
    SetMinSize(wxSize(s_minimumWindowWidth, s_minimumWindowHeight));
    
    m_device = std::make_unique<Device>();
    if (!m_device) {
        wxMessageBox("m_device is nullptr?", "Error", wxICON_ERROR);
        return;
    }
}

void MainFrame::OnEnterRecovery(wxCommandEvent&) {
    if (!m_lockdowndClient || !m_lockdowndClient->IsOpen()) {
        wxMessageBox("Cannot connect to lockdownd!", "Error", wxICON_ERROR);
        return;
    }
    
    bool success = m_lockdowndClient->EnterRecoveryMode(m_sessionID);
    if (success) {
#if _WIN32
        wxMessageBox("Sent device to recovery mode! If Antares does not see your device once it is in recovery mode, make sure that its driver is set to libusbK in Zadig.", "Success", wxICON_INFORMATION);
#else
        wxMessageBox("Sent device to recovery mode!", "Success", wxICON_INFORMATION);
#endif
    } else {
        wxMessageBox("Failed to send device to recovery mode!", "Error", wxICON_ERROR);
    }
}

void MainFrame::OnExitRecovery(wxCommandEvent&) {
    if (!m_device) {
        wxMessageBox("Cannot connect to device!", "Error", wxICON_ERROR);
        return;
    }
    
    auto mode = m_device->GetMode();
    if (!mode.has_value()) {
        wxMessageBox("Cannot determine device mode!", "Error", wxICON_ERROR);
        return;
    }
    
    if (*mode != Device::Mode::Recovery) {
        wxMessageBox("Device is not in recovery mode!", "Error", wxICON_ERROR);
        return;
    }

    int confirm = wxMessageBox(
        "Do you want to boot device with verbose logs?",
        "Verbose boot?",
        wxYES_NO | wxICON_QUESTION
    );

    if (confirm == wxYES) {
        m_device->SendCommand("setenv boot-args \"rd=disk0s1 -v\"\n");
    } else {
        m_device->SendCommand("setenv boot-args \"rd=disk0s1\"\n");
    }
    m_device->SendCommand("setenv boot-partition 0\n");
    m_device->SendCommand("setenv auto-boot true\n");
    m_device->SendCommand("saveenv\n");
    m_device->SendCommand("reboot\n");

    wxMessageBox("Your device should now be exiting recovery.", "Status", wxICON_INFORMATION);
}

void MainFrame::OnCustomBootCommands(wxCommandEvent&) {
    int confirm = wxMessageBox(
        "This is an option intended for people who want to input custom boot commands. Please make sure you know what you are doing! Are you sure you want to continue?",
        "Warning",
        wxYES_NO | wxICON_WARNING
    );

    if (confirm != wxYES) {
        return;
    }

    while (true) {
        wxTextEntryDialog dialog(this, "Enter a command to send to the device.\nPress cancel to stop.", "Send custom command");
        
        if (dialog.ShowModal() != wxID_OK) {
            break;
        }
        
        std::string command = dialog.GetValue().ToStdString();

        m_device->SendCommand(command + "\n");
        
        if (command == "fsboot" || command == "reboot" || command == "reset" || command == "poweroff") {
            break;
        }
    }
}

void MainFrame::OnJailbreak(wxCommandEvent&) {
    if (m_lockdowndClient && m_lockdowndClient->IsOpen()) {
        bool success = m_lockdowndClient->EnterRecoveryMode(m_sessionID);
        if (!success) {
            wxMessageBox("Failed to send recovery message to device!", "Error", wxICON_ERROR);
            return;
        }

        m_lockdowndClient->Close();
        
        const auto deadline = std::chrono::steady_clock::now() + RecoveryTimeout;
        
        bool deviceOpened = false;
        do {
            deviceOpened = m_device->Open();
            if (deviceOpened) {
                auto currentMode = m_device->GetMode();
                if (currentMode.has_value() && *currentMode == Device::Mode::Recovery) {
                    break;
                }
            }
        } while (std::chrono::steady_clock::now() < deadline); 

        if (!deviceOpened) {
#if _WIN32
            wxMessageBox("Failed to send device to recovery mode. Make sure the device's driver is set to libusbK in Zadig.", "Error", wxICON_ERROR);
#else
            wxMessageBox("Failed to send device to recovery mode. Please try again.", "Error", wxICON_ERROR);
#endif
            return;
        }
    }

    auto mode = m_device->GetMode();
    if (!mode.has_value()) {
        wxMessageBox("Failed to get device mode.", "Error", wxICON_ERROR);
        return;
    }

    if (*mode != Device::Mode::Recovery) {
        wxMessageBox("Device is not in recovery mode.", "Error", wxICON_ERROR);
        return;
    }

    m_device->SendCommand("setpicture 0\n");
    m_device->SendCommand("bgcolor 125 125 0\n");
    
    auto ramdiskData = LoadFile("core/ramdisk/ramdisk.img");
    if (!ramdiskData.has_value()) {
        wxMessageBox("Failed to load ramdisk.img!", "Error", wxICON_ERROR);
        m_device->SendCommand("bgcolor 125 0 0\n");
        return;
    }
    
    if (!m_device->SendFile(*ramdiskData, 0x09CC2000)) {
        wxMessageBox("Failed to send ramdisk!", "Error", wxICON_ERROR);
        m_device->SendCommand("bgcolor 125 0 0\n");
        return;
    }

    if (m_verboseBootCheckbox->IsChecked()) {
        m_device->SendCommand("setenv antares_verbose_boot \"1\"\n");
    } else {
        m_device->SendCommand("setenv antares_verbose_boot \"0\"\n");
    }

    m_device->SendCommand("bgcolor 0 125 0\n");

    m_device->SendCommand("setenv boot-args \"rd=md0 -s -x pmd0=0x09CC2000.0x0133D000\"\n");
    m_device->SendCommand("saveenv\n");
    m_device->SendCommand("fsboot\n");
}

void MainFrame::OnTimerPoll(wxTimerEvent&) {
    if (m_lockdowndClient && m_lockdowndClient->IsOpen()) {
        m_enterRecoveryItem->Enable(true);
        m_exitRecoveryItem->Enable(false);
        m_customBootCommandsItem->Enable(false);
        return;
    }
    
    if (m_lockdowndClient) {
        m_lockdowndClient.reset();
        m_device->Close();

        m_statusText->SetLabel("No device connected.");
        m_jailbreakButton->Enable(false);

        RefreshUI();
        
        return;
    }
    
    bool successfullyOpened = m_device->Open();
    if (!successfullyOpened) {
        m_enterRecoveryItem->Enable(false);
        m_exitRecoveryItem->Enable(false);
        m_customBootCommandsItem->Enable(false);

        m_statusText->SetLabel("No device connected.");
        m_jailbreakButton->Enable(false);

        RefreshUI();
        return;
    }

    if (!m_device->IsSupported()) {
        m_enterRecoveryItem->Enable(false);
        m_exitRecoveryItem->Enable(false);
        m_customBootCommandsItem->Enable(false);

        m_statusText->SetLabel("Unsupported device.");
        m_jailbreakButton->Enable(false);

        RefreshUI();
        return;
    }

    auto mode = m_device->GetMode();
    if (!mode.has_value()) {
        m_enterRecoveryItem->Enable(false);
        m_exitRecoveryItem->Enable(false);
        m_customBootCommandsItem->Enable(false);

        m_statusText->SetLabel("Failed to get device mode");
        m_jailbreakButton->Enable(false);

        RefreshUI();
        return;
    }

    switch (*mode) {
        case Device::Mode::Normal: {
            m_device->Close();

            m_lockdowndClient = std::make_unique<LockdownDaemonClient>();

            if (!m_lockdowndClient->Open()) {
                m_enterRecoveryItem->Enable(false);
                m_exitRecoveryItem->Enable(false);
                m_customBootCommandsItem->Enable(false);

                m_lockdowndClient.reset();

                m_statusText->SetLabel("Device connected in normal mode, but failed to open connection.");
                m_jailbreakButton->Enable(false);
                RefreshUI();

                return;
            }

            std::string sessionError;
            auto sessionID = m_lockdowndClient->StartPairedSession(sessionError);
            if (!sessionID.has_value()) {
                m_enterRecoveryItem->Enable(false);
                m_exitRecoveryItem->Enable(false);
                m_customBootCommandsItem->Enable(false);

                m_lockdowndClient.reset();

                m_statusText->SetLabel("Failed to start paired session: " + sessionError + ".");
                m_jailbreakButton->Enable(false);
                RefreshUI();

                return;
            }

            m_enterRecoveryItem->Enable(true);
            m_exitRecoveryItem->Enable(false);
            m_customBootCommandsItem->Enable(false);

            m_jailbreakButton->Enable(true);
            
            m_sessionID = *sessionID;
            
            auto productType = m_lockdowndClient->GetValueString("ProductType");
            auto productVersion = m_lockdowndClient->GetValueString("ProductVersion");
            
            if (productType.has_value() && productVersion.has_value()) {
                auto marketingName = GetMarketingProductName(*productType);
                m_statusText->SetLabel(wxString(marketingName.value_or(*productType) + " on iPhone OS " + *productVersion + " connected in normal mode!"));
            } else {
                m_statusText->SetLabel("Failed to fetch device type or version");
            }
        } break;
        
        case Device::Mode::Recovery: {
            m_enterRecoveryItem->Enable(false);
            m_exitRecoveryItem->Enable(true);
            m_customBootCommandsItem->Enable(true);

            m_statusText->SetLabel("Device connected in recovery mode!");
            m_jailbreakButton->Enable(true);
        } break;
    }

    RefreshUI();
}

void MainFrame::RefreshUI(void) {
    m_statusText->Wrap(s_minimumWindowWidth - (30 * 2));

    Refresh();
    Layout();
}

wxHyperlinkCtrl *MainFrame::MakeCustomHyperlink(const wxString& name, const wxString& link) {
    auto *hyperlink = new wxHyperlinkCtrl(
        this,
        wxID_ANY,
        name,
        link
    );
    hyperlink->SetNormalColour(wxColour(138, 180, 248));
    hyperlink->SetHoverColour(wxColour(174, 153, 250));
    hyperlink->SetVisitedColour(wxColour(150, 140, 255));
    return hyperlink;
}

std::optional<std::string> MainFrame::GetMarketingProductName(std::string_view productType) {
    if (productType == "iPod1,1") {
        return "iPod touch (1st generation)";
    } else if (productType == "iPhone1,1") {
        return "iPhone (1st generation)";
    }
    
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> MainFrame::LoadFile(const std::string& path) {
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