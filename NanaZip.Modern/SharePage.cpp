#include "pch.h"
#include "SharePage.h"
#if __has_include("SharePage.g.cpp")
#include "SharePage.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::Modern::implementation
{
    SharePage::SharePage(
        _In_ HWND WindowHandle,
        _In_ std::vector<std::wstring> const& SharingFilePaths) :
        m_WindowHandle(WindowHandle),
        m_SharingInterop(SharingFilePaths){ }

    void SharePage::InitializeComponent()
    {
        SharePageT::InitializeComponent();

        this->ShareButton().Click([this](
            winrt::IInspectable const&,
            winrt::RoutedEventArgs const&)
            {
                this->m_SharingInterop.SharingByDataTransferManager(
                    this->m_WindowHandle);
            });

        this->EmailButton().Click([this](
            winrt::IInspectable const&,
            winrt::RoutedEventArgs const&)
            {
                this->m_SharingInterop.SharingByWindowsMessaging(
                    this->m_WindowHandle);
            });

        this->CancelButton().Click([this](
            winrt::IInspectable const&,
            winrt::RoutedEventArgs const&)
            {
                ::check_bool(::DestroyWindow(this->m_WindowHandle));
            });
    }
}
