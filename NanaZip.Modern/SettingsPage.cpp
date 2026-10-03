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

        auto SwitchElement = sender.as<winrt::ToggleSwitch>();

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
            SwitchElement.IsOn(SwitchValue != false);
        }

        SwitchElement.Toggled(
            [this](winrt::IInspectable const& sender,
                   winrt::RoutedEventArgs const& e)
            {
                UNREFERENCED_PARAMETER(e);

                auto SwitchElement = sender.as<winrt::ToggleSwitch>();

                DWORD SwitchValue = SwitchElement.IsOn();
                DWORD SwitchValueLength = sizeof(SwitchValue);

                std::wstring SubKey = L"Software\\NanaZip\\";
                std::wstring SwitchTag =
                    SwitchElement.Tag().as<winrt::hstring>().c_str();
                SubKey.append(SwitchTag);

                LSTATUS Result = ::RegSetKeyValueW(
                    HKEY_CURRENT_USER,
                    SubKey.c_str(),
                    SwitchElement.Name().c_str(),
                    REG_DWORD,
                    reinterpret_cast<PVOID>(&SwitchValue),
                    SwitchValueLength);

                if (ERROR_SUCCESS == Result
                    && SwitchTag.compare(L"FM") == 0)
                {
                    HWND MainWindowHandle =
                        ::GetWindow(this->m_WindowHandle, GW_OWNER);
                    ::PostMessageW(
                        MainWindowHandle,
                        WM_COMMAND,
                        K7_MAIN_WINDOW_COMMAND_REFRESH_ALL_PANELS,
                        0);
                }
            });
    }

    void SettingsPage::ComboBoxLoading(
        winrt::FrameworkElement const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(e);

        auto ComboElement = sender.as<winrt::ComboBox>();

        DWORD ComboValue = ComboElement.SelectedIndex();
        DWORD ComboValueLength = sizeof(ComboValue);

        std::wstring SubKey = L"Software\\NanaZip\\";
        SubKey.append(ComboElement.Tag().as<winrt::hstring>());

        LSTATUS Result = ::RegGetValueW(
            HKEY_CURRENT_USER,
            SubKey.c_str(),
            ComboElement.Name().c_str(),
            RRF_RT_REG_DWORD,
            nullptr,
            reinterpret_cast<PVOID>(&ComboValue),
            &ComboValueLength);

        if (ERROR_SUCCESS == Result
            && ComboValue < ComboElement.Items().Size())
        {
            ComboElement.SelectedIndex(static_cast<std::int32_t>(ComboValue));
        }

        ComboElement.SelectionChanged(
            [](winrt::IInspectable sender, winrt::SelectionChangedEventArgs e)
            {
                UNREFERENCED_PARAMETER(e);

                auto ComboElement = sender.as<winrt::ComboBox>();

                DWORD ComboValue = ComboElement.SelectedIndex();
                DWORD ComboValueLength = sizeof(ComboValue);

                std::wstring SubKey = L"Software\\NanaZip\\";
                SubKey.append(ComboElement.Tag().as<winrt::hstring>());

                ::RegSetKeyValueW(
                    HKEY_CURRENT_USER,
                    SubKey.c_str(),
                    ComboElement.Name().c_str(),
                    REG_DWORD,
                    reinterpret_cast<PVOID>(&ComboValue),
                    ComboValueLength);
            });
    }

    void SettingsPage::ContextMenuListViewLoading(
        winrt::FrameworkElement const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(e);
        winrt::ListView ListViewElement = sender.as<winrt::ListView>();

        DWORD ContextMenuValue = 0x0;
        DWORD ContextMenuValueLength = sizeof(ContextMenuValue);

        std::wstring SubKey = L"Software\\NanaZip\\";
        SubKey.append(ListViewElement.Tag().as<winrt::hstring>());

        LSTATUS Result = ::RegGetValueW(
            HKEY_CURRENT_USER,
            SubKey.c_str(),
            ListViewElement.Name().c_str(),
            RRF_RT_REG_DWORD,
            nullptr,
            reinterpret_cast<PVOID>(&ContextMenuValue),
            &ContextMenuValueLength);

        if (ERROR_SUCCESS == Result)
        {
            auto SelectedListViewItems = ListViewElement.SelectedItems();
            SelectedListViewItems.Clear();
            for (auto const& it : ListViewElement.Items())
            {
                auto ListViewItemElement = it.as<winrt::ListViewItem>();
                auto ListViewItemFlag =
                    ListViewItemElement.Tag().as<std::int32_t>();
                DWORD IsSelectedValue =
                    ContextMenuValue & (1 << ListViewItemFlag);
                if (IsSelectedValue)
                {
                    SelectedListViewItems.Append(ListViewItemElement);
                }
            }
        }

        ListViewElement.SelectionChanged(
            [](winrt::IInspectable sender, winrt::SelectionChangedEventArgs e)
            {
                UNREFERENCED_PARAMETER(e);
                winrt::ListView ListViewElement = sender.as<winrt::ListView>();

                DWORD ContextMenuValue = 0x0;
                DWORD ContextMenuValueLength = sizeof(ContextMenuValue);

                for (auto const& it : ListViewElement.SelectedItems())
                {
                    auto ListViewItemElement = it.as<winrt::ListViewItem>();
                    auto ListViewItemFlag =
                        ListViewItemElement.Tag().as<std::int32_t>();
                    ContextMenuValue |= (1 << ListViewItemFlag);
                }

                std::wstring SubKey = L"Software\\NanaZip\\";
                SubKey.append(ListViewElement.Tag().as<winrt::hstring>());

                ::RegSetKeyValueW(
                    HKEY_CURRENT_USER,
                    SubKey.c_str(),
                    ListViewElement.Name().c_str(),
                    REG_DWORD,
                    reinterpret_cast<PVOID>(&ContextMenuValue),
                    ContextMenuValueLength);
            });
    }
}
