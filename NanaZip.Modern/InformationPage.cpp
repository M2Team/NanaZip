#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    InformationPage::InformationPage(
        _In_opt_ HWND WindowHandle,
        winrt::hstring const& WindowTitle,
        winrt::hstring const& WindowContent) :
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
		::SendMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
