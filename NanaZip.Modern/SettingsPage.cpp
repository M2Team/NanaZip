#include "pch.h"
#include "SettingsPage.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "NanaZip.Modern.h"

#include <K7User.h>

#include <ShObjIdl_core.h>

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
            [](winrt::IInspectable const& sender,
               winrt::SelectionChangedEventArgs const& e)
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
            [](winrt::IInspectable const& sender,
               winrt::SelectionChangedEventArgs const& e)
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

    void SettingsPage::AutoSuggestBoxLoading(
        winrt::FrameworkElement const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(e);
        winrt::AutoSuggestBox AutoSuggestBoxElement =
            sender.as<winrt::AutoSuggestBox>();

        std::wstring SubKey = L"Software\\NanaZip\\";
        SubKey.append(AutoSuggestBoxElement.Tag().as<winrt::hstring>());

        DWORD AutoSuggestBoxValueLength = 0;
        LSTATUS Result = ::RegGetValueW(
            HKEY_CURRENT_USER,
            SubKey.c_str(),
            AutoSuggestBoxElement.Name().c_str(),
            RRF_RT_REG_SZ,
            nullptr,
            nullptr,
            &AutoSuggestBoxValueLength);
        if (ERROR_SUCCESS == Result)
        {
            std::wstring AutoSuggestBoxValue(AutoSuggestBoxValueLength, L'\0');
            Result = ::RegGetValueW(
                HKEY_CURRENT_USER,
                SubKey.c_str(),
                AutoSuggestBoxElement.Name().c_str(),
                RRF_RT_REG_SZ,
                nullptr,
                reinterpret_cast<PVOID>(AutoSuggestBoxValue.data()),
                &AutoSuggestBoxValueLength);

            if (ERROR_SUCCESS == Result)
            {
                AutoSuggestBoxElement.Text(AutoSuggestBoxValue);
            }
        }

        AutoSuggestBoxElement.TextChanged(
            [](winrt::AutoSuggestBox const& sender,
               winrt::AutoSuggestBoxTextChangedEventArgs const& args)
            {
                if (winrt::AutoSuggestionBoxTextChangeReason::ProgrammaticChange
                    != args.Reason())
                {
                    std::wstring AutoSuggestBoxText = sender.Text().c_str();
                    DWORD AutoSuggestBoxTextLength =
                        static_cast<DWORD>((AutoSuggestBoxText.size() + 1)
                            * sizeof(wchar_t));

                    std::wstring SubKey = L"Software\\NanaZip\\";
                    SubKey.append(sender.Tag().as<winrt::hstring>());

                    if (args.CheckCurrent())
                    {
                        ::RegSetKeyValueW(
                            HKEY_CURRENT_USER,
                            SubKey.c_str(),
                            sender.Name().c_str(),
                            REG_SZ,
                            reinterpret_cast<PVOID>(AutoSuggestBoxText.data()),
                            AutoSuggestBoxTextLength);
                    }
                }
            });
    }

    void SettingsPage::OpenFileDialogToSelectPath(
        winrt::AutoSuggestBox const& sender,
        winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto Dialog = winrt::create_instance<::IFileOpenDialog>(
            CLSID_FileOpenDialog,
            CLSCTX_LOCAL_SERVER);
        Dialog->SetOptions(
            FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        if (ERROR_SUCCESS == Dialog->Show(this->m_WindowHandle))
        {
            winrt::com_ptr<IShellItem> Item = nullptr;
            if (ERROR_SUCCESS == Dialog->GetResult(Item.put()))
            {
                LPWSTR Path = nullptr;
                Item->GetDisplayName(SIGDN_FILESYSPATH, &Path);

                sender.Text(Path);

                std::wstring SubKey = L"Software\\NanaZip\\";
                SubKey.append(sender.Tag().as<winrt::hstring>());

                ::RegSetKeyValueW(
                    HKEY_CURRENT_USER,
                    SubKey.c_str(),
                    sender.Name().c_str(),
                    REG_SZ,
                    reinterpret_cast<PVOID>(Path),
                    static_cast<DWORD>(
                        (::wcslen(Path) + 1) * sizeof(wchar_t)));

                ::CoTaskMemFree(Path);
            }
        }
    }

    void SettingsPage::OpenFileDialogToSelectExecutable(
        winrt::AutoSuggestBox const& sender,
        winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto Dialog = winrt::create_instance<::IFileOpenDialog>(
            CLSID_FileOpenDialog,
            CLSCTX_LOCAL_SERVER);

        COMDLG_FILTERSPEC Types[] =
        {
            { L"*.EXE", L"*.EXE"}
        };

        Dialog->SetFileTypes(static_cast<UINT>(std::size(Types)), Types);
        Dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);

        if (ERROR_SUCCESS == Dialog->Show(this->m_WindowHandle))
        {
            winrt::com_ptr<IShellItem> Item = nullptr;
            if (ERROR_SUCCESS == Dialog->GetResult(Item.put()))
            {
                LPWSTR Path = nullptr;
                Item->GetDisplayName(SIGDN_FILESYSPATH, &Path);

                sender.Text(Path);

                std::wstring SubKey = L"Software\\NanaZip\\";
                SubKey.append(sender.Tag().as<winrt::hstring>());

                ::RegSetKeyValueW(
                    HKEY_CURRENT_USER,
                    SubKey.c_str(),
                    sender.Name().c_str(),
                    REG_SZ,
                    reinterpret_cast<PVOID>(Path),
                    static_cast<DWORD>(
                        (::wcslen(Path) + 1) * sizeof(wchar_t)));

                ::CoTaskMemFree(Path);
            }
        }
    }
}
