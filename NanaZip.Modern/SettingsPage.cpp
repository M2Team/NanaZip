#include "pch.h"
#include "SettingsPage.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "NanaZip.Modern.h"
#include "ThemeManager.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

#include <ShObjIdl_core.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.System.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

namespace winrt::NanaZip::Modern::implementation
{
    SettingsPage::SettingsPage(
        _In_opt_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {
        winrt::hstring WindowTitle = Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/SettingsPage/TitleTextBlock/Text");
        ::SetWindowTextW(this->m_WindowHandle, WindowTitle.c_str());

        HICON ApplicationIconHandle = reinterpret_cast<HICON>(::LoadImageW(
            ::GetModuleHandleW(nullptr),
            MAKEINTRESOURCE(IDI_ICON),
            IMAGE_ICON,
            256,
            256,
            LR_SHARED));
        if (ApplicationIconHandle)
        {
            ::SendMessageW(
                this->m_WindowHandle,
                WM_SETICON,
                TRUE,
                reinterpret_cast<LPARAM>(ApplicationIconHandle));
            ::SendMessageW(
                this->m_WindowHandle,
                WM_SETICON,
                FALSE,
                reinterpret_cast<LPARAM>(ApplicationIconHandle));
        }
    }

    void SettingsPage::InitializeComponent()
    {
        SettingsPageT::InitializeComponent();
        LoadCurrentTheme();
        m_IsInitialized = true;

        // Set version text
        try
        {
            auto package = winrt::Windows::ApplicationModel::Package::Current();
            auto version = package.Id().Version();
            VersionTextBlock().Text = L"NanaZip " +
                winrt::to_hstring(version.Major) + L"." +
                winrt::to_hstring(version.Minor) + L"." +
                winrt::to_hstring(version.Build) + L"." +
                winrt::to_hstring(version.Revision);
        }
        catch (...)
        {
            VersionTextBlock().Text = L"NanaZip";
        }
    }

    void SettingsPage::LoadCurrentTheme()
    {
        if (!ThemeComboBox())
        {
            return;
        }

        auto& themeManager = NanaZip::Modern::ThemeManager::Instance();
        auto currentTheme = themeManager.CurrentTheme();

        int selectedIndex = 1; // Default to Glassy
        switch (currentTheme)
        {
        case NanaZip::Modern::AppTheme::Transparent:
            selectedIndex = 0;
            break;
        case NanaZip::Modern::AppTheme::Glassy:
            selectedIndex = 1;
            break;
        case NanaZip::Modern::AppTheme::Matte:
            selectedIndex = 2;
            break;
        }

        ThemeComboBox().SelectedIndex(selectedIndex);
    }

    void SettingsPage::ThemeComboBox_SelectionChanged(
        winrt::IInspectable const& sender,
        winrt::SelectionChangedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(e);

        if (!m_IsInitialized || !ThemeComboBox())
        {
            return;
        }

        auto selectedItem = ThemeComboBox().SelectedItem().as<winrt::Windows::UI::Xaml::Controls::ComboBoxItem>();
        if (!selectedItem)
        {
            return;
        }

        auto tag = selectedItem.Tag();
        if (!tag)
        {
            return;
        }

        int32_t themeValue = winrt::unbox_value<int32_t>(tag);
        auto& themeManager = NanaZip::Modern::ThemeManager::Instance();
        themeManager.SetTheme(static_cast<NanaZip::Modern::AppTheme>(themeValue));
    }

    void SettingsPage::GitHubButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = {};
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile = L"https://github.com/M2Team/NanaZip/";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }

    void SettingsPage::LicenseButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = {};
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile = L"https://github.com/M2Team/NanaZip/blob/main/LICENSE";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }
}