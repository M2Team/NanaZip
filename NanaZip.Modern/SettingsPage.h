#pragma once

#include "SettingsPage.g.h"

#include <Windows.h>

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
}

namespace winrt::NanaZip::Modern::implementation
{
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

        void OpenFileDialogToSelectPath(
            winrt::AutoSuggestBox const& sender,
            winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args);

        void OpenFileDialogToSelectExecutable(
            winrt::AutoSuggestBox const& sender,
            winrt::AutoSuggestBoxQuerySubmittedEventArgs const& args);

    private:

        HWND m_WindowHandle;
    };
}
