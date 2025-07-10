#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    InformationPage::InformationPage(
        HWND WindowHandle,
        winrt::hstring WindowTitle,
        winrt::hstring WindowContent) :
        m_WindowHandle(WindowHandle),
        m_WindowTitle(WindowTitle),
        m_WindowContent(WindowContent)
    {
    }

    void InformationPage::InitializeComponent()
    {
        InformationPageT::InitializeComponent();

        ::SetWindowTextW(
            this->m_WindowHandle,
            this->m_WindowTitle.c_str()
        );
        
        this->TitleTextBlock().Text(this->m_WindowTitle);
        this->InformationTextBox().Text(this->m_WindowContent);
    }

    void InformationPage::CloseButtonClickedHandler(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);
        DestroyWindow(this->m_WindowHandle);
    }
}
