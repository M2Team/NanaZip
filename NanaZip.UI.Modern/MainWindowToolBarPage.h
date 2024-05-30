#pragma once

#include "MainWindowToolBarPage.g.h"

#include <Windows.h>

#include <winrt/Windows.System.h>
#include <winrt/Windows.Services.Store.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.Foundation.Numerics.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::Services::Store::StoreContext;
    using Windows::System::DispatcherQueue;
    using Windows::UI::Xaml::RoutedEventArgs;
    using Windows::UI::Composition::Compositor;
    using Windows::UI::Composition::CompositionClip;
    using Windows::UI::Composition::CompositionRoundedRectangleGeometry;
    using Windows::UI::Composition::SpriteVisual;
    using Windows::UI::Composition::ShapeVisual;
    using Windows::UI::Composition::CompositionVisualSurface;
    using Windows::UI::Composition::CompositionBrush;
    using Windows::UI::Composition::Compositor;
    using Windows::Foundation::Numerics::float2;
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

        winrt::CompositionRoundedRectangleGeometry m_sponsorButtonShadowClipGeometry = nullptr;
        winrt::CompositionRoundedRectangleGeometry m_sponsorButtonShadowMaskGeometry = nullptr;
        winrt::CompositionRoundedRectangleGeometry m_sponsorButtonVisualMaskGeometry = nullptr;
        winrt::ShapeVisual m_sponsorButtonVisualMaskShapeVisual = nullptr;
        winrt::ShapeVisual m_sponsorButtonShadowMaskShapeVisual = nullptr;
        winrt::SpriteVisual m_sponsorButtonShadowSpriteVisual = nullptr;
        winrt::CompositionVisualSurface m_sponsorButtonVisualMaskVisualSurface = nullptr;
        winrt::CompositionVisualSurface m_sponsorButtonShadowMaskVisualSurface = nullptr;
        winrt::CompositionVisualSurface m_sponsorButtonShadowVisualSurface = nullptr;

        bool CheckSponsorEditionLicense();

        void RefreshSponsorButtonContent();

        void CreateSponsorButtonShadow();

        winrt::CompositionClip GetShadowClip(winrt::Compositor compositor, float CornerRadius);

        winrt::CompositionBrush CreateVisualOpacityMask(winrt::Compositor compositor, float CornerRadius);

        winrt::CompositionBrush GetShadowMask(winrt::Compositor compositor, float CornerRadius);
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<
        MainWindowToolBarPage, implementation::MainWindowToolBarPage>
    {
    };
}
