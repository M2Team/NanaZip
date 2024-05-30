#include "pch.h"
#include "MainWindowToolBarPage.h"
#if __has_include("MainWindowToolBarPage.g.cpp")
#include "MainWindowToolBarPage.g.cpp"
#endif

#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include "../SevenZip/CPP/Common/Common.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/resource.h"
#include "../SevenZip/CPP/7zip/UI/FileManager/LangUtils.h"

#include "NanaZip.UI.h"

#include <ShObjIdl_core.h>

namespace
{
    DWORD GetShellProcessId()
    {
        HWND ShellWindowHandle = ::GetShellWindow();
        if (!ShellWindowHandle)
        {
            return static_cast<DWORD>(-1);
        }

        DWORD ShellProcessId = static_cast<DWORD>(-1);
        if (!::GetWindowThreadProcessId(ShellWindowHandle, &ShellProcessId))
        {
            return static_cast<DWORD>(-1);
        }
        return ShellProcessId;
    }

    std::wstring GetCurrentProcessModulePath()
    {
        // 32767 is the maximum path length without the terminating null character.
        std::wstring Path(32767, L'\0');
        Path.resize(::GetModuleFileNameW(
            nullptr, &Path[0], static_cast<DWORD>(Path.size())));
        return Path;
    }
}

namespace winrt
{
    using Windows::Foundation::Point;
    using Windows::Foundation::IAsyncAction;
    using Windows::Services::Store::StoreProduct;
    using Windows::Services::Store::StoreProductQueryResult;
    using Windows::System::DispatcherQueuePriority;
    using Windows::UI::Xaml::Automation::AutomationProperties;
    using Windows::UI::Xaml::Controls::AppBarButton;
    using Windows::UI::Xaml::Controls::ToolTipService;
    using Windows::UI::Xaml::Media::GeneralTransform;
}

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace
{
    namespace ToolBarCommandID
    {
        enum
        {
            Add = 1070,
            Extract = 1071,
            Test = 1072,
        };
    }

    namespace MenuIndex
    {
        enum
        {
            File = 0,
            Edit,
            View,
            Bookmarks
        };
    }
}

namespace winrt::NanaZip::Modern::implementation
{
    MainWindowToolBarPage::MainWindowToolBarPage(
        _In_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {

    }

    void MainWindowToolBarPage::InitializeComponent()
    {
        MainWindowToolBarPageT::InitializeComponent();

        DWORD ToolBarResources[10] =
        {
            IDS_ADD,
            IDS_EXTRACT,
            IDS_TEST,
            IDS_BUTTON_COPY,
            IDS_BUTTON_MOVE,
            IDS_BUTTON_DELETE,
            IDS_BUTTON_INFO,
            IDM_OPTIONS,
            IDM_BENCHMARK,
            IDM_ABOUT
        };

        winrt::AppBarButton ToolBarButtons[10] =
        {
            this->AddButton(),
            this->ExtractButton(),
            this->TestButton(),
            this->CopyButton(),
            this->MoveButton(),
            this->DeleteButton(),
            this->InfoButton(),
            this->OptionsButton(),
            this->BenchmarkButton(),
            this->AboutButton()
        };

        const std::size_t ToolBarButtonCount =
            sizeof(ToolBarButtons) / sizeof(*ToolBarButtons);

        for (size_t i = 0; i < ToolBarButtonCount; ++i)
        {
            std::wstring Resource = std::wstring(
                ::LangString(ToolBarResources[i]));
            winrt::AutomationProperties::SetName(
                ToolBarButtons[i],
                Resource);
            winrt::ToolTipService::SetToolTip(
                ToolBarButtons[i],
                winrt::box_value(Resource));
        }

        this->m_DispatcherQueue =
            winrt::DispatcherQueue::GetForCurrentThread();

        this->m_StoreContext = winrt::StoreContext::GetDefault();
        if (this->m_StoreContext)
        {
            winrt::check_hresult(
                this->m_StoreContext.as<IInitializeWithWindow>()->Initialize(
                    this->m_WindowHandle));
        }
    }

    void MainWindowToolBarPage::PageLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        // Create the necessary shadows.
        this->CreateSponsorButtonShadow();

        this->RefreshSponsorButtonContent();
    }

    const float MaxBlurRadius = 72;
    const float BlurRadius = 24;

    void MainWindowToolBarPage::CreateSponsorButtonShadow()
    {
        const float CornerRadius = 5;

        auto Compositor = Window::Current().Compositor();

        // If the resource for OpacityMaskShapeVisualSurfaceBrushResourceKey exists it means this.InnerContentClipMode == CompositionVisualSurface,
        // which means we need to take some steps to set up an opacity mask.

        // Create a CompositionVisualSurface, and use the SpriteVisual containing the shadow as the source.
        this->m_sponsorButtonShadowVisualSurface = Compositor.CreateVisualSurface();
        this->m_sponsorButtonShadowVisualSurface.SourceVisual(this->m_sponsorButtonShadowSpriteVisual = Compositor.CreateSpriteVisual());

        this->m_sponsorButtonShadowSpriteVisual.Clip(this->GetShadowClip(Compositor, CornerRadius));
        this->m_sponsorButtonShadowSpriteVisual.RelativeSizeAdjustment(float2::zero());
        this->m_sponsorButtonShadowSpriteVisual.Size(this->SponsorButton().ActualSize());

        Windows::UI::Composition::DropShadow dropShadow = nullptr;
        this->m_sponsorButtonShadowSpriteVisual.Shadow(dropShadow = Compositor.CreateDropShadow());
        dropShadow.Mask(this->GetShadowMask(Compositor, CornerRadius));
        dropShadow.BlurRadius(14);
        dropShadow.Opacity(0.3f);
        dropShadow.Color(Windows::UI::Colors::Black());

        // Adjust the offset and size of the CompositionVisualSurface to accommodate the thick outline of the shape created in UpdateVisualOpacityMask().
        this->m_sponsorButtonShadowVisualSurface.SourceOffset(float2(-72));
        this->m_sponsorButtonShadowVisualSurface.SourceSize(this->SponsorButton().ActualSize() + float2(MaxBlurRadius * 2));

        // Create a CompositionSurfaceBrush from the CompositionVisualSurface. This allows us to render the shadow in a brush.
        Windows::UI::Composition::CompositionSurfaceBrush shadowSurfaceBrush = Compositor.CreateSurfaceBrush();
        shadowSurfaceBrush.Surface(this->m_sponsorButtonShadowVisualSurface);
        shadowSurfaceBrush.Stretch(Windows::UI::Composition::CompositionStretch::None);

        // Create a CompositionMaskBrush, using the CompositionSurfaceBrush of the shadow as the source,
        // and the CompositionSurfaceBrush created in UpdateVisualOpacityMask() as the mask.
        // This creates a brush that renders the shadow with its inner portion clipped out.
        Windows::UI::Composition::CompositionMaskBrush maskBrush = Compositor.CreateMaskBrush();
        maskBrush.Source(shadowSurfaceBrush);
        maskBrush.Mask(this->CreateVisualOpacityMask(Compositor, CornerRadius));

        // Create a SpriteVisual and set its brush to the CompositionMaskBrush created in the previous step,
        // then set it as the child of the element in the context.
        SpriteVisual visual = Compositor.CreateSpriteVisual();
        visual.RelativeSizeAdjustment(float2::one());
        visual.Offset(Windows::Foundation::Numerics::float3(-72, -72, 0));
        visual.Size(float2(MaxBlurRadius * 2));
        visual.Brush(maskBrush);

        Windows::UI::Xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(this->SponsorButton(), visual);

        this->SponsorButton().SizeChanged([this, CornerRadius](const auto&, const SizeChangedEventArgs& e) {
            float2 Size = e.NewSize();
            this->m_sponsorButtonVisualMaskGeometry.Size(Size + float2(MaxBlurRadius));
            auto sharedSize = Size + float2(MaxBlurRadius * 2);
            this->m_sponsorButtonVisualMaskShapeVisual.Size(sharedSize);
            this->m_sponsorButtonVisualMaskVisualSurface.SourceSize(sharedSize);
            this->m_sponsorButtonShadowMaskGeometry.Size(Size);
            this->m_sponsorButtonShadowMaskVisualSurface.SourceSize(Size);
            this->m_sponsorButtonShadowMaskShapeVisual.Size(Size);
            this->m_sponsorButtonShadowSpriteVisual.Size(Size);
            this->m_sponsorButtonShadowVisualSurface.SourceSize(Size + float2(MaxBlurRadius * 2));
            this->m_sponsorButtonShadowClipGeometry.Size(
                float2(
                    Size.x + CornerRadius * 2,
                    Size.y + CornerRadius * 2
                )
            );
        });
    }

    CompositionBrush MainWindowToolBarPage::CreateVisualOpacityMask(Compositor Compositor, float CornerRadius)
    {
        this->m_sponsorButtonVisualMaskShapeVisual = Compositor.CreateShapeVisual();

        this->m_sponsorButtonVisualMaskGeometry = Compositor.CreateRoundedRectangleGeometry();
        Windows::UI::Composition::CompositionSpriteShape shape = Compositor.CreateSpriteShape(this->m_sponsorButtonVisualMaskGeometry);

        // Set the attributes of the geometry, and add the CompositionSpriteShape to the ShapeVisual.
        // The geometry will have a thick outline and no fill, meaning that when used as a mask,
        // the shadow will only be rendered on the outer area covered by the outline, clipping out its inner portion.
        this->m_sponsorButtonVisualMaskGeometry.Offset(float2(MaxBlurRadius / 2));
        this->m_sponsorButtonVisualMaskGeometry.CornerRadius(float2(MaxBlurRadius / 2 + CornerRadius));
        shape.StrokeThickness(72);
        shape.StrokeBrush(Compositor.CreateColorBrush(Windows::UI::Colors::Black()));

        this->m_sponsorButtonVisualMaskShapeVisual.Shapes().Append(shape);

        // Create CompositionVisualSurface using the ShapeVisual as the source visual.
        this->m_sponsorButtonVisualMaskVisualSurface = Compositor.CreateVisualSurface();
        this->m_sponsorButtonVisualMaskVisualSurface.SourceVisual(this->m_sponsorButtonVisualMaskShapeVisual);

        this->m_sponsorButtonVisualMaskGeometry.Size(SponsorButton().ActualSize() + float2(MaxBlurRadius));
        auto sharedSize = SponsorButton().ActualSize() + float2(MaxBlurRadius * 2);
        this->m_sponsorButtonVisualMaskShapeVisual.Size(sharedSize);
        this->m_sponsorButtonVisualMaskVisualSurface.SourceSize(sharedSize);

        // Create a CompositionSurfaceBrush using the CompositionVisualSurface as the source, this essentially converts the ShapeVisual into a brush.
        // This brush can then be used as a mask.
        Windows::UI::Composition::CompositionSurfaceBrush opacityMask = Compositor.CreateSurfaceBrush();
        opacityMask.Surface(this->m_sponsorButtonVisualMaskVisualSurface);
        return opacityMask;
    }

    CompositionBrush MainWindowToolBarPage::GetShadowMask(Compositor Compositor, float CornerRadius)
    {
        // Create rounded rectangle geometry and add it to a shape
        this->m_sponsorButtonShadowMaskGeometry = Compositor.CreateRoundedRectangleGeometry();
        this->m_sponsorButtonShadowMaskGeometry.CornerRadius(float2(CornerRadius));
        auto shape = Compositor.CreateSpriteShape(m_sponsorButtonShadowMaskGeometry);
        shape.FillBrush(Compositor.CreateColorBrush(Windows::UI::Colors::Black()));
        // Create a ShapeVisual so that our geometry can be rendered to a visual
        this->m_sponsorButtonShadowMaskShapeVisual = Compositor.CreateShapeVisual();
        this->m_sponsorButtonShadowMaskShapeVisual.Shapes().Append(shape);
        // Create a CompositionVisualSurface, which renders our ShapeVisual to a texture
        this->m_sponsorButtonShadowMaskVisualSurface = Compositor.CreateVisualSurface();
        this->m_sponsorButtonShadowMaskVisualSurface.SourceVisual(this->m_sponsorButtonShadowMaskShapeVisual);

        // Create a CompositionSurfaceBrush to render our CompositionVisualSurface to a brush.
        // Now we have a rounded rectangle brush that can be used on as the mask for our shadow.
        auto surfaceBrush = Compositor.CreateSurfaceBrush(this->m_sponsorButtonShadowMaskVisualSurface);
        this->m_sponsorButtonShadowMaskGeometry.Size(SponsorButton().RenderSize());
        this->m_sponsorButtonShadowMaskVisualSurface.SourceSize(SponsorButton().RenderSize());
        this->m_sponsorButtonShadowMaskShapeVisual.Size(SponsorButton().RenderSize());
        return surfaceBrush;
    }

    CompositionClip MainWindowToolBarPage::GetShadowClip(Compositor Compositor, float CornerRadius)
    {
        // The way this shadow works without the need to project on another element is because
        // we're clipping the inner part of the shadow which would be cast on the element
        // itself away. This method is creating an outline so that we are only showing the
        // parts of the shadow that are outside the element's context.
        // Note: This does cause an issue if the element does clip itself to its bounds, as then
        // the shadowed area is clipped as well.
        this->m_sponsorButtonShadowClipGeometry = Compositor.CreateRoundedRectangleGeometry();

        this->m_sponsorButtonShadowClipGeometry.Offset(float2(-CornerRadius));
        this->m_sponsorButtonShadowClipGeometry.Size(
            float2(
                SponsorButton().ActualSize().x + CornerRadius * 2,
                SponsorButton().ActualSize().y + CornerRadius * 2
            )
        );
        this->m_sponsorButtonShadowClipGeometry.CornerRadius(float2(CornerRadius));

        return Compositor.CreateGeometricClip(this->m_sponsorButtonShadowClipGeometry);
    }

    void MainWindowToolBarPage::AddButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Add,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::ExtractButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Extract,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::TestButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Test,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::CopyButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                IDM_COPY_TO,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::MoveButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                IDM_MOVE_TO,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::DeleteButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                IDM_DELETE,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::InfoButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                IDM_PROPERTIES,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::OptionsButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                IDM_OPTIONS,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::BenchmarkButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                IDM_BENCHMARK,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::AboutButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        NanaZip::UI::ShowAboutDialog(this->m_WindowHandle);
    }

    void MainWindowToolBarPage::MoreButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        winrt::AppBarButton Button = sender.as<winrt::AppBarButton>();

        winrt::GeneralTransform Transform =
            Button.TransformToVisual(this->Content());
        winrt::Point LogicalPoint = Transform.TransformPoint(
            winrt::Point(0.0f, 0.0f));

        extern HMENU g_MoreMenu;

        UINT DpiValue = ::GetDpiForWindow(this->m_WindowHandle);

        POINT MenuPosition = { 0 };
        MenuPosition.x = ::MulDiv(
            static_cast<int>(LogicalPoint.X),
            DpiValue,
            USER_DEFAULT_SCREEN_DPI);
        MenuPosition.y = ::MulDiv(
            48,
            DpiValue,
            USER_DEFAULT_SCREEN_DPI);
        ::MapWindowPoints(
            this->m_WindowHandle,
            HWND_DESKTOP,
            &MenuPosition,
            1);

        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                g_MoreMenu,
                MenuIndex::File)),
            MenuIndex::File);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                g_MoreMenu,
                MenuIndex::Edit)),
            MenuIndex::Edit);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                g_MoreMenu,
                MenuIndex::View)),
            MenuIndex::View);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                g_MoreMenu,
                MenuIndex::Bookmarks)),
            MenuIndex::Bookmarks);

        WPARAM Command = ::TrackPopupMenuEx(
            g_MoreMenu,
            TPM_LEFTALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
            MenuPosition.x,
            MenuPosition.y,
            this->m_WindowHandle,
            nullptr);
        if (Command)
        {
            ::PostMessageW(this->m_WindowHandle, WM_COMMAND, Command, 0);
        }
    }

    void MainWindowToolBarPage::SponsorButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        winrt::handle(Mile::CreateThread([=]()
        {
            DWORD ShellProcessId = ::GetShellProcessId();
            if (static_cast<DWORD>(-1) == ShellProcessId)
            {
                return;
            }

            HANDLE ShellProcessHandle = nullptr;

            auto Handler = Mile::ScopeExitTaskHandler([&]()
            {
                if (ShellProcessHandle)
                {
                    ::CloseHandle(ShellProcessHandle);
                }
            });

            ShellProcessHandle = ::OpenProcess(
                PROCESS_CREATE_PROCESS,
                FALSE,
                ShellProcessId);
            if (!ShellProcessHandle)
            {
                return;
            }

            SIZE_T AttributeListSize = 0;
            ::InitializeProcThreadAttributeList(
                nullptr,
                1,
                0,
                &AttributeListSize);

            std::vector<std::uint8_t> AttributeListBuffer =
                std::vector<std::uint8_t>(AttributeListSize);

            PPROC_THREAD_ATTRIBUTE_LIST AttributeList =
                reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
                    &AttributeListBuffer[0]);

            if (!::InitializeProcThreadAttributeList(
                AttributeList,
                1,
                0,
                &AttributeListSize))
            {
                return;
            }

            if (!::UpdateProcThreadAttribute(
                AttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                &ShellProcessHandle,
                sizeof(ShellProcessHandle),
                nullptr,
                nullptr))
            {
                return;
            }

            STARTUPINFOEXW StartupInfoEx = { 0 };
            PROCESS_INFORMATION ProcessInformation = { 0 };
            StartupInfoEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
            StartupInfoEx.lpAttributeList = AttributeList;

            std::wstring ApplicationName = ::GetCurrentProcessModulePath();

            if (!::CreateProcessW(
                ApplicationName.c_str(),
                const_cast<LPWSTR>(L"NanaZip --AcquireSponsorEdition"),
                nullptr,
                nullptr,
                TRUE,
                CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                nullptr,
                nullptr,
                &StartupInfoEx.StartupInfo,
                &ProcessInformation))
            {
                return;
            }

            ::CloseHandle(ProcessInformation.hThread);
            ::WaitForSingleObjectEx(ProcessInformation.hProcess, INFINITE, FALSE);
            ::CloseHandle(ProcessInformation.hProcess);

            ::RegDeleteKeyValueW(
                HKEY_CURRENT_USER,
                L"Software\\NanaZip",
                L"SponsorEdition");

            this->RefreshSponsorButtonContent();
        }));
    }

    bool MainWindowToolBarPage::CheckSponsorEditionLicense()
    {
        {
            DWORD Data = 0;
            DWORD Length = sizeof(DWORD);
            if (ERROR_SUCCESS == ::RegGetValueW(
                HKEY_CURRENT_USER,
                L"Software\\NanaZip",
                L"SponsorEdition",
                RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
                nullptr,
                &Data,
                &Length))
            {
                return Data;
            }
        }

        bool Sponsored = false;

        if (this->m_StoreContext)
        {
            winrt::StoreProductQueryResult ProductQueryResult =
                this->m_StoreContext.GetStoreProductsAsync(
                    { L"Durable" },
                    { L"9N9DNPT6D6Z9" }).get();
            for (auto Item : ProductQueryResult.Products())
            {
                winrt::StoreProduct Product = Item.Value();
                Sponsored = Product.IsInUserCollection();
            }
        }

        {
            DWORD Data = Sponsored;
            ::RegSetKeyValueW(
                HKEY_CURRENT_USER,
                L"Software\\NanaZip",
                L"SponsorEdition",
                REG_DWORD,
                &Data,
                sizeof(DWORD));
        }

        return Sponsored;
    }

    void MainWindowToolBarPage::RefreshSponsorButtonContent()
    {
        winrt::handle(Mile::CreateThread([=]()
        {
            bool Sponsored = this->CheckSponsorEditionLicense();

            if (!this->m_DispatcherQueue)
            {
                return;
            }
            this->m_DispatcherQueue.TryEnqueue(
                winrt::DispatcherQueuePriority::Normal,
                [=]()
            {
                this->SponsorButton().Content(
                    winrt::box_value(Mile::WinRT::GetLocalizedString(
                        Sponsored
                        ? L"MainWindowToolBarPage/SponsorButton/SponsoredText"
                        : L"MainWindowToolBarPage/SponsorButton/AcquireText")));
            });
        }));
    }
}
