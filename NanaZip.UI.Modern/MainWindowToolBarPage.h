#pragma once

#include "MainWindowToolBarPage.g.h"

#include <Windows.h>

#include <winrt/Windows.System.h>
#include <winrt/Windows.Services.Store.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::Services::Store::StoreContext;
    using Windows::System::DispatcherQueue;
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

        void PageLoaded(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void AddButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ExtractButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void TestButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void CopyButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MoveButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void DeleteButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void InfoButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OptionsButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void BenchmarkButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void AboutButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MoreButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void SponsorButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        HWND m_WindowHandle;
        winrt::DispatcherQueue m_DispatcherQueue = nullptr;
        winrt::StoreContext m_StoreContext = nullptr;

        bool CheckSponsorEditionLicense();

        void RefreshSponsorButtonContent();
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<
        MainWindowToolBarPage, implementation::MainWindowToolBarPage>
    {
    };
}
