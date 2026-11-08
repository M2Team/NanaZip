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
    std::optional<std::uint32_t> SettingsPage::GetWinRegDWordValue(
        std::wstring const& SubKey,
        std::wstring const& Value)
    {
        DWORD Data = 0;
        DWORD DataLength = sizeof(Data);
        LSTATUS Result = ::RegGetValueW(
            this->m_NanaZipRegKey.get(),
            SubKey.c_str(),
            Value.c_str(),
            RRF_RT_REG_DWORD,
            nullptr,
            static_cast<PVOID>(&Data),
            &DataLength);
        if (Result == ERROR_SUCCESS)
        {
            return Data;
        }
        return std::nullopt;
    }

    std::optional<std::wstring> SettingsPage::GetWinRegStringValue(
        std::wstring const& SubKey,
        std::wstring const& Value)
    {
        DWORD DataLength = 0;
        LSTATUS GetLengthResult = ::RegGetValueW(
            this->m_NanaZipRegKey.get(),
            SubKey.c_str(),
            Value.c_str(),
            RRF_RT_REG_SZ,
            nullptr,
            nullptr,
            &DataLength);
        if (GetLengthResult == ERROR_SUCCESS)
        {
            std::wstring Data(DataLength / sizeof(wchar_t) - 1, L'\0');
            LSTATUS GetDataResult = ::RegGetValueW(
                this->m_NanaZipRegKey.get(),
                SubKey.c_str(),
                Value.c_str(),
                RRF_RT_REG_SZ,
                nullptr,
                static_cast<PVOID>(Data.data()),
                &DataLength);
            if (GetDataResult == ERROR_SUCCESS)
            {
                return Data;
            }
        }
        return std::nullopt;
    }

    void SettingsPage::SetWinRegDWordValue(
        std::wstring const& SubKey,
        std::wstring const& Value,
        std::uint32_t Data)
    {
        DWORD DataLength = sizeof(Data);
        ::RegSetKeyValueW(
            this->m_NanaZipRegKey.get(),
            SubKey.c_str(),
            Value.c_str(),
            REG_DWORD,
            static_cast<PVOID>(&Data),
            DataLength);
    }

    void SettingsPage::SetWinRegStringValue(
        std::wstring const& SubKey,
        std::wstring const& Value,
        std::wstring const& Data)
    {
        auto DataLength =
            static_cast<DWORD>((Data.size() + 1) * sizeof(wchar_t));
        ::RegSetKeyValueW(
            this->m_NanaZipRegKey.get(),
            SubKey.c_str(),
            Value.c_str(),
            REG_SZ,
            static_cast<PVOID>(const_cast<wchar_t*>(Data.data())),
            DataLength);
    }

    SettingsPage::SettingsPage(
        _In_opt_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {
        ::RegCreateKeyW(
            HKEY_CURRENT_USER,
            L"Software\\NanaZip\\",
            m_NanaZipRegKey.put());
    }

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
        auto RegData = this->GetWinRegDWordValue(
            SwitchElement.Tag().as<winrt::hstring>().c_str(),
            SwitchElement.Name().c_str());
        if (RegData.has_value())
        {
            SwitchElement.IsOn(static_cast<bool>(RegData.value()));
        }

        SwitchElement.Toggled(
            [this](winrt::IInspectable const& sender,
                   winrt::RoutedEventArgs const& e)
            {
                UNREFERENCED_PARAMETER(e);

                auto SwitchElement = sender.as<winrt::ToggleSwitch>();
                std::wstring SwitchTag =
                    SwitchElement.Tag().as<winrt::hstring>().c_str();
                this->SetWinRegDWordValue(
                    SwitchTag,
                    SwitchElement.Name().c_str(),
                    SwitchElement.IsOn());
                if (SwitchTag.compare(L"FM") == 0)
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
        auto RegData = this->GetWinRegDWordValue(
            ComboElement.Tag().as<winrt::hstring>().c_str(),
            ComboElement.Name().c_str());
        if (RegData.has_value())
        {
            ComboElement.SelectedIndex(
                static_cast<std::int32_t>(RegData.value()));
        }

        ComboElement.SelectionChanged(
            [this](winrt::IInspectable const& sender,
               winrt::SelectionChangedEventArgs const& e)
            {
                UNREFERENCED_PARAMETER(e);

                auto ComboElement = sender.as<winrt::ComboBox>();
                this->SetWinRegDWordValue(
                    ComboElement.Tag().as<winrt::hstring>().c_str(),
                    ComboElement.Name().c_str(),
                    ComboElement.SelectedIndex());
            });
    }

    void SettingsPage::ContextMenuListViewLoading(
        winrt::FrameworkElement const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(e);
        auto ListViewElement = sender.as<winrt::ListView>();
        auto RegData = this->GetWinRegDWordValue(
            ListViewElement.Tag().as<winrt::hstring>().c_str(),
            ListViewElement.Name().c_str());
        if (RegData.has_value())
        {
            auto SelectedListViewItems = ListViewElement.SelectedItems();
            SelectedListViewItems.Clear();
            for (auto const& it : ListViewElement.Items())
            {
                auto ListViewItemElement = it.as<winrt::ListViewItem>();
                auto ListViewItemFlag =
                    ListViewItemElement.Tag().as<std::int32_t>();
                DWORD IsSelectedValue =
                    RegData.value() & (1 << ListViewItemFlag);
                if (IsSelectedValue)
                {
                    SelectedListViewItems.Append(ListViewItemElement);
                }
            }
        }

        ListViewElement.SelectionChanged(
            [this](winrt::IInspectable const& sender,
               winrt::SelectionChangedEventArgs const& e)
            {
                UNREFERENCED_PARAMETER(e);
                winrt::ListView ListViewElement = sender.as<winrt::ListView>();
                DWORD ContextMenuValue = 0x0;
                for (auto const& it : ListViewElement.SelectedItems())
                {
                    auto ListViewItemElement = it.as<winrt::ListViewItem>();
                    auto ListViewItemFlag =
                        ListViewItemElement.Tag().as<std::int32_t>();
                    ContextMenuValue |= (1 << ListViewItemFlag);
                }
                this->SetWinRegDWordValue(
                    ListViewElement.Tag().as<winrt::hstring>().c_str(),
                    ListViewElement.Name().c_str(),
                    ContextMenuValue);
            });
    }

    void SettingsPage::AutoSuggestBoxLoading(
        winrt::FrameworkElement const& sender,
        winrt::IInspectable const& e)
    {
        UNREFERENCED_PARAMETER(e);
        winrt::AutoSuggestBox AutoSuggestBoxElement =
            sender.as<winrt::AutoSuggestBox>();
        auto RegData = this->GetWinRegStringValue(
            AutoSuggestBoxElement.Tag().as<winrt::hstring>().c_str(),
            AutoSuggestBoxElement.Name().c_str());
        if (RegData.has_value())
        {
            AutoSuggestBoxElement.Text(RegData.value());
        }
        AutoSuggestBoxElement.TextChanged(
            [this](winrt::AutoSuggestBox const& sender,
               winrt::AutoSuggestBoxTextChangedEventArgs const& args)
            {
                //if (winrt::AutoSuggestionBoxTextChangeReason::ProgrammaticChange
                //    != args.Reason())
                //{
                std::wstring AutoSuggestBoxText = sender.Text().c_str();
                if (args.CheckCurrent())
                {
                    this->SetWinRegStringValue(
                        sender.Tag().as<winrt::hstring>().c_str(),
                        sender.Name().c_str(),
                        AutoSuggestBoxText);
                }
                //}
            });
    }

    void SettingsPage::OpenFileDialogToSelectPath(
        winrt::AutoSuggestBox const& sender,
        winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto Dialog = winrt::create_instance<::IFileOpenDialog>(
            CLSID_FileOpenDialog,
            CLSCTX::CLSCTX_INPROC_SERVER);
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
                ::CoTaskMemFree(Path);
            }
        }
    }

    std::optional<std::wstring> SettingsPage::ShowFileDialog(
        std::wstring const& FileTypes,
        FILEOPENDIALOGOPTIONS Options)
    {
        COMDLG_FILTERSPEC FilterSpecs[] =
        {
            { FileTypes.c_str(), FileTypes.c_str() }
        };

        winrt::guid CLSID_FileOpenDialogBroker(
            L"7394A3BB-E76A-4244-8534-132593937AD1");
        auto Dialog = winrt::try_create_instance<IFileOpenDialog>(
            CLSID_FileOpenDialogBroker,
            CLSCTX::CLSCTX_LOCAL_SERVER);
        bool UseOopFileDialog = true;
        if (Dialog == nullptr)
        {
        fallback: Dialog = winrt::try_create_instance<IFileOpenDialog>(
            CLSID_FileOpenDialog,
            CLSCTX::CLSCTX_INPROC_SERVER);
            UseOopFileDialog = false;
        }

        Dialog->SetFileTypes(
            static_cast<UINT>(std::size(FilterSpecs)),
            FilterSpecs);
        Dialog->SetOptions(
            Options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);

        switch (Dialog->Show(this->m_WindowHandle))
        {
        case S_OK:
        {
            winrt::com_ptr<IShellItem> Item = nullptr;
            switch (Dialog->GetResult(Item.put()))
            {
            case S_OK:
            case ::HRESULT_FROM_WIN32(ERROR_NOT_APPCONTAINER):
                // OopFileDialog will throw ERROR_NOT_APPCONTAINER, but success in fact.
            {
                WinShellStr Path;
                Item->GetDisplayName(SIGDN_FILESYSPATH, Path.put());
                return Path.get();
            }
            default:
                return std::nullopt;
            }
        }
        case ::HRESULT_FROM_WIN32(ERROR_CANCELLED):
            return std::nullopt;

        case E_FAIL: // OopFileDialog can't show in Administrator Context.
            if (UseOopFileDialog)
            {
                goto fallback;
            } // Don't break, InpFileDialog default.
        default:
            return std::nullopt;
        }
    }

    void SettingsPage::OpenFileDialogToSelectExecutable(
        winrt::AutoSuggestBox const& sender,
        winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        auto Path = this->ShowFileDialog(L"*.EXE", 0);
        if (Path.has_value())
        {
            sender.Text(Path.value());
        }
    }
}
