#pragma once

#include "MainWindowToolBarPage.g.h"

#include <Windows.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}

namespace winrt::NanaZip::Modern::implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<MainWindowToolBarPage>
    {
    public:

        MainWindowToolBarPage(
            _In_ HWND WindowHandle = nullptr);

        void InitializeComponent();

    private:

        HWND m_WindowHandle;
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<
        MainWindowToolBarPage, implementation::MainWindowToolBarPage>
    {
    };
}
