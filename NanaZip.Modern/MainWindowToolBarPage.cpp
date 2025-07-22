#include "pch.h"
#include "MainWindowToolBarPage.h"
#if __has_include("MainWindowToolBarPage.g.cpp")
#include "MainWindowToolBarPage.g.cpp"
#endif

#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include "NanaZip.Modern.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

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
            Copy = 546,
            Move = 547,
            Delete = 548,
            Info = 551,
            Options = 900,
            Benchmark = 901,
            About = 961
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
        _In_ HWND WindowHandle,
        _In_ HMENU MoreMenu) :
        m_WindowHandle(WindowHandle),
        m_MoreMenu(MoreMenu)
    {

    }

    void MainWindowToolBarPage::InitializeComponent()
    {
        MainWindowToolBarPageT::InitializeComponent();

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

        const wchar_t* ToolBarResources[10] =
        {
            L"Legacy/Resource7200",
            L"Legacy/Resource7201",
            L"Legacy/Resource7202",
            L"Legacy/Resource7203",
            L"Legacy/Resource7204",
            L"Legacy/Resource7205",
            L"Legacy/Resource7206",
            L"Legacy/Resource900",
            L"Legacy/Resource901",
            L"Legacy/Resource961"
        };

        const std::size_t ToolBarButtonCount =
            sizeof(ToolBarButtons) / sizeof(*ToolBarButtons);

        for (size_t i = 0; i < ToolBarButtonCount; ++i)
        {
            winrt::hstring Resource =
                Mile::WinRT::GetLocalizedString(ToolBarResources[i]);
            winrt::AutomationProperties::SetName(
                ToolBarButtons[i],
                Resource);
            winrt::ToolTipService::SetToolTip(
                ToolBarButtons[i],
                winrt::box_value(Resource));
        }

        this->m_DispatcherQueue =
            winrt::DispatcherQueue::GetForCurrentThread();

        std::wstring sponsorButtonLabel = L"[";
        sponsorButtonLabel += Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/MainWindowToolBarPage/SponsorButton/AcquireText");
        sponsorButtonLabel += L"]";

        this->SponsorButton().Content(winrt::box_value(sponsorButtonLabel));
    }

    void MainWindowToolBarPage::PageLoaded(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        this->RefreshSponsorButtonContent();
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
                ToolBarCommandID::Copy,
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
                ToolBarCommandID::Move,
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
                ToolBarCommandID::Delete,
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
                ToolBarCommandID::Info,
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
                ToolBarCommandID::Options,
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
                ToolBarCommandID::Benchmark,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::AboutButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::About,
                BN_CLICKED),
            0);
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
                this->m_MoreMenu,
                MenuIndex::File)),
            MenuIndex::File);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::Edit)),
            MenuIndex::Edit);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::View)),
            MenuIndex::View);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::Bookmarks)),
            MenuIndex::Bookmarks);

        WPARAM Command = ::TrackPopupMenuEx(
            this->m_MoreMenu,
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

            ::AllowSetForegroundWindow(
                ::GetProcessId(ProcessInformation.hProcess));

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

        if (!this->m_StoreContext)
        {
            this->m_StoreContext = winrt::StoreContext::GetDefault();
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
                std::wstring ResourcePath = L"NanaZip.Modern/";
                ResourcePath += L"MainWindowToolBarPage/SponsorButton/";
                ResourcePath += Sponsored ? L"SponsoredText" : L"AcquireText";
                winrt::hstring ResourceContent =
                    Mile::WinRT::GetLocalizedString(ResourcePath.c_str());
                this->SponsorButton().Content(
                    winrt::box_value(ResourceContent));
            });
        }));
    }
}

EXTERN_C LPVOID WINAPI K7ModernCreateMainWindowToolBarPage(
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MoreMenuHandle)
{
    using Interface =
        winrt::NanaZip::Modern::MainWindowToolBarPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::MainWindowToolBarPage;

    Interface Window = winrt::make<Implementation>(
        ParentWindowHandle,
        MoreMenuHandle);
    return winrt::detach_abi(Window);
}
