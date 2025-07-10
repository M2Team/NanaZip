#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    InformationPage::InformationPage(
        HWND windowHandle,
        LPCWSTR title,
        LPCWSTR text)
        : m_WindowHandle(windowHandle),
        m_Title(title),
        m_Text(text)
    {
    }

    void InformationPage::InitializeComponent()
    {
        InformationPageT::InitializeComponent();
        
        this->TitleTextBlock().Text(m_Title);
        this->InformationTextBox().Text(m_Text);
    }

    void InformationPage::CloseButtonClickedHandler(
        winrt::IInspectable const&,
        winrt::RoutedEventArgs const&)
    {
        DestroyWindow(m_WindowHandle);
    }
}
