#pragma once

#include "AboutPage.g.h"

#include <Windows.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::FrameworkElement;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Xaml::Controls::ToggleSwitch;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct AboutPage : AboutPageT<AboutPage>
    {
    public:

        AboutPage(
            _In_opt_ HWND WindowHandle = nullptr,
            _In_opt_ LPCWSTR ExtendedMessage = nullptr);

        void InitializeComponent();

        void FileAssociationsButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ToggleSwitchLoading(
            winrt::FrameworkElement const& sender,
            winrt::IInspectable const& e);

        void ToggleSwitchToggled(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void NanaZipWebsiteButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void GitHubButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void CancelButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle;
        std::wstring m_ExtendedMessage;
    };
}
