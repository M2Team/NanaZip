#pragma once

#include "SharePage.g.h"

#include <Windows.h>

#include "ShareExchange.h"

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct SharePage : SharePageT<SharePage>
    {
    public:

        SharePage(
            _In_ HWND WindowHandle,
            _In_ std::vector<std::wstring> const& SharingFilePaths);

        void InitializeComponent();

    private:

        HWND m_WindowHandle;
        K7ShareExchangeInterop m_SharingInterop;
    };
}

//namespace winrt::NanaZip::Modern::factory_implementation
//{
//    struct SharePage :
//        SharePageT<SharePage, implementation::SharePage>
//    {
//    };
//}
