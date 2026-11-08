#pragma once

#include "SettingsPage.g.h"

#include <Windows.h>
#include <ShlObj_core.h>

#include <optional>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::FrameworkElement;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Xaml::Controls::ToggleSwitch;
    using Windows::UI::Xaml::Controls::ComboBox;
    using Windows::UI::Xaml::Controls::SelectionChangedEventArgs;
    using Windows::UI::Xaml::Controls::ListView;
    using Windows::UI::Xaml::Controls::ListViewItem;
    using Windows::UI::Xaml::Controls::SelectionChangedEventArgs;
    using Windows::UI::Xaml::Controls::AutoSuggestBox;
    using Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs;
    using Windows::UI::Xaml::Controls::AutoSuggestionBoxTextChangeReason;
    using Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs;
    using Windows::UI::Xaml::Visibility;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct regkey_traits
    {
        using type = HKEY;
        static void close(type value) noexcept
        {
            ::RegCloseKey(value);
        }
        static constexpr type invalid() noexcept
        {
            return nullptr;
        }
    };
    using WinRegKey = handle_type<regkey_traits>;

    struct shellstr_traits
    {
        using type = LPWSTR;
        static void close(type value) noexcept
        {
            ::CoTaskMemFree(value);
        }
        static constexpr type invalid() noexcept
        {
            return nullptr;
        }
    };
    using WinShellStr = handle_type<shellstr_traits>;

    struct SettingsPage : SettingsPageT<SettingsPage>
    {
    public:

        SettingsPage(
            _In_opt_ HWND WindowHandle = nullptr);

        void InitializeComponent();

        void FileAssociationsButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ToggleSwitchLoading(
            winrt::FrameworkElement const& sender,
            winrt::IInspectable const& e);

        void ComboBoxLoading(
            winrt::FrameworkElement const& sender,
            winrt::IInspectable const& e);

        void ContextMenuListViewLoading(
            winrt::FrameworkElement const& sender,
            winrt::IInspectable const& e);

        void AutoSuggestBoxLoading(
            winrt::FrameworkElement const& sender,
            winrt::IInspectable const& e);

        void OpenFileDialogToSelectFolder(
            winrt::AutoSuggestBox const& sender,
            winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args);

        void OpenFileDialogToSelectExecutable(
            winrt::AutoSuggestBox const& sender,
            winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args);

    private:

        HWND m_WindowHandle;

        WinRegKey m_NanaZipRegKey;

        std::optional<std::uint32_t> GetWinRegDWordValue(
            std::wstring const& SubKey,
            std::wstring const& Value);

        std::optional<std::wstring> GetWinRegStringValue(
            std::wstring const& SubKey,
            std::wstring const& Value);

        void SetWinRegDWordValue(
            std::wstring const& SubKey,
            std::wstring const& Value,
            std::uint32_t Data);

        void SetWinRegStringValue(
            std::wstring const& SubKey,
            std::wstring const& Value,
            std::wstring const& Data);

        std::optional<std::wstring> ShowFileDialog(
            std::wstring const& FileTypes,
            FILEOPENDIALOGOPTIONS Options);

        void GetGroupPolicyValue();
    };
}
