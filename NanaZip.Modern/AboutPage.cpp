#include "pch.h"
#include "AboutPage.h"
#if __has_include("AboutPage.g.cpp")
#include "AboutPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <K7User.h>

#include <winrt/Windows.UI.Xaml.Documents.h>

#include <Mile.Project.Version.h>

namespace winrt::NanaZip::Modern::implementation
{
    AboutPage::AboutPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR ExtendedMessage) :
        m_WindowHandle(WindowHandle),
        m_ExtendedMessage(ExtendedMessage)
    {

    }

    void AboutPage::InitializeComponent()
    {
        AboutPageT::InitializeComponent();

        winrt::hstring WindowTitle = winrt::hstring(
            ::K7ModernGetLegacyStringResource(2900));
        if (WindowTitle.empty())
        {
            WindowTitle = L"About NanaZip";
        }
        ::SetWindowTextW(this->m_WindowHandle, WindowTitle.c_str());

        std::wstring Version = std::wstring(
            "NanaZip " MILE_PROJECT_VERSION_STRING);
        Version.append(
            L" (" MILE_PROJECT_DOT_VERSION_STRING L")");
#if defined(_M_AMD64)
        Version.append(L" (x64)");
#elif defined(_M_ARM64)
        Version.append(L" (arm64)");
#endif

        std::wstring Content = std::wstring(
            ::K7ModernGetLegacyStringResource(2901));
        if (Content.empty())
        {
            Content = L"NanaZip is free software";
        }
        if (!this->m_ExtendedMessage.empty())
        {
            Content.append(L"\r\n\r\n");
            Content.append(this->m_ExtendedMessage);
        }

        this->GridTitleTextBlock().Text(WindowTitle);
        this->Version().Text(Version);
        this->Content().Text(Content);
    }

    void AboutPage::FileAssociationsButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::K7UserModernLaunchDefaultAppsSettings();
    }

    void AboutPage::ToggleSwitchLoading(
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
        SwitchElement.Toggled({ this->get_strong(), &AboutPage::ToggleSwitchToggled});
    }

    void AboutPage::ToggleSwitchToggled(
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

    void AboutPage::NanaZipWebsiteButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = {};
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile = L"https://nanazip.org";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }

    void AboutPage::GitHubButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = {};
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile = L"https://github.com/M2Team/NanaZip";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }

    void AboutPage::CancelButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
