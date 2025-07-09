#pragma once

#include "InformationPage.g.h"
#include "ControlMacros.h"
#include <Mile.Helpers.CppWinRT.h>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::UI::Xaml;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct InformationPage : InformationPageT<InformationPage>
    {
        InformationPage(HWND windowHandle);

        DEPENDENCY_PROPERTY_HEADER(
            Text,
            winrt::hstring
        );

        void CloseButtonClickedHandler(winrt::IInspectable const& sender, winrt::RoutedEventArgs const& args);

    private:
        HWND m_WindowHandle{ nullptr };
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct InformationPage : InformationPageT<InformationPage, implementation::InformationPage>
    {
    };
}
