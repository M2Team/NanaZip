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
        AboutPage(
            _In_ HWND WindowHandle = nullptr);

        void InitializeComponent();

    private:

        HWND m_WindowHandle;
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct AboutPage :
        AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
