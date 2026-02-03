#include "pch.h"
#include "InformationPage.h"
#if __has_include("InformationPage.g.cpp")
#include "InformationPage.g.cpp"
#endif

namespace winrt::NanaZip::Modern::implementation
{
    InformationPage::InformationPage(
        _In_opt_ HWND WindowHandle,
        _In_opt_ LPCWSTR Title,
        _In_opt_ LPCWSTR Content) :
        m_WindowHandle(WindowHandle),
        m_Title(Title),
        m_Content(Content)
    {
    }

    void InformationPage::InitializeComponent()
    {
        InformationPageT::InitializeComponent();

        ::SetWindowTextW(this->m_WindowHandle, this->m_Title.c_str());
        
        this->TitleTextBlock().Text(this->m_Title);
        this->InformationTextBox().Text(this->m_Content);
    }

    void InformationPage::CloseButtonClickedHandler(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

		::PostMessageW(this->m_WindowHandle, WM_CLOSE, 0, 0);
    }
}
