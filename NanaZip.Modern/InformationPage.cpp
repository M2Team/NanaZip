#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        Text,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::InformationPage,
        winrt::NanaZip::Modern::InformationPage,
        L""
    );

    void InformationPage::CloseButtonClickedHandler(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
    }
}
