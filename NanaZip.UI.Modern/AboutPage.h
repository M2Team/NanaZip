#pragma once

#include "AboutPage.g.h"

#include <Windows.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
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

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct AboutPage :
        AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
