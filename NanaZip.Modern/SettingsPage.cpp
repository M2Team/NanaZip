#include "pch.h"
#include "SettingsPage.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <K7User.h>

namespace winrt::NanaZip::Modern::implementation
{
    SettingsPage::SettingsPage(
        _In_opt_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle){ }

    void SettingsPage::InitializeComponent()
    {
        SettingsPageT::InitializeComponent();

        winrt::hstring WindowTitle = winrt::hstring(
            ::K7ModernGetLegacyStringResource(2900));
        if (WindowTitle.empty())
        {
            WindowTitle = L"About NanaZip";
        }
        ::SetWindowTextW(this->m_WindowHandle, WindowTitle.c_str());
    }

    void SettingsPage::FileAssociationsButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::K7UserModernLaunchDefaultAppsSettings();
    }

    void SettingsPage::ToggleSwitchLoading(
        winrt::FrameworkElement const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(e);

        winrt::ToggleSwitch SwitchElement = sender.as<winrt::ToggleSwitch>();

        DWORD SwitchValue = SwitchElement.IsOn();
        DWORD SwitchValueLength = sizeof(SwitchValue);

        std::wstring SubKey = L"Software\\NanaZip\\";
        SubKey.append(SwitchElement.Tag().as<winrt::hstring>());

        LSTATUS Result = ::RegGetValueW(
            HKEY_CURRENT_USER,
            SubKey.c_str(),
            SwitchElement.Name().c_str(),
            RRF_RT_REG_DWORD,
            nullptr,
            reinterpret_cast<PVOID>(&SwitchValue),
            &SwitchValueLength);

        if (ERROR_SUCCESS == Result)
        {
            SwitchElement.IsOn(static_cast<bool>(SwitchValue));
        }

        // Prevent ToggleSwitchToggled when Loading.
        SwitchElement.Toggled({ this->get_strong(), &SettingsPage::ToggleSwitchToggled});
    }

    void SettingsPage::ToggleSwitchToggled(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        winrt::ToggleSwitch SwitchElement = sender.as<winrt::ToggleSwitch>();

        DWORD SwitchValue = SwitchElement.IsOn();
        DWORD SwitchValueLength = sizeof(SwitchValue);

        std::wstring SubKey = L"Software\\NanaZip\\";
        SubKey.append(SwitchElement.Tag().as<winrt::hstring>());

        ::RegSetKeyValueW(
            HKEY_CURRENT_USER,
            SubKey.c_str(),
            SwitchElement.Name().c_str(),
            REG_DWORD,
            reinterpret_cast<PVOID>(&SwitchValue),
            SwitchValueLength);
    }
}
