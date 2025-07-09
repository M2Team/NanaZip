#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    InformationPage::InformationPage(HWND windowHandle)
        : m_WindowHandle(windowHandle)
    {
    }

    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        Text,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::InformationPage,
        winrt::NanaZip::Modern::InformationPage,
        L""
    );
    DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(
        Title,
        winrt::hstring,
        winrt::NanaZip::Modern::implementation::InformationPage,
        winrt::NanaZip::Modern::InformationPage,
        L"Information"
    );

    void InformationPage::CloseButtonClickedHandler(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        DestroyWindow(m_WindowHandle);
    }
}
