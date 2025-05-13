#include "pch.h"
#include "SponsorPage.h"
#if __has_include("SponsorPage.g.cpp")
#include "SponsorPage.g.cpp"
#endif

#include "NanaZip.ModernExperience.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

#include <ShObjIdl_core.h>

#include <winrt/Windows.Services.Store.h>

// Defined in SevenZip/CPP/7zip/UI/FileManager/resourceGui.h
#define IDI_ICON 1

namespace winrt
{
    using Windows::Services::Store::StoreContext;
    using Windows::Services::Store::StoreProduct;
    using Windows::Services::Store::StoreProductQueryResult;
}

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::NanaZip::ModernExperience::implementation
{
    SponsorPage::SponsorPage(
        _In_ HWND WindowHandle) :
        m_WindowHandle(WindowHandle)
    {
        ::SetWindowTextW(
            this->m_WindowHandle,
            Mile::WinRT::GetLocalizedString(
                L"NanaZip.ModernExperience/SponsorPage/GridTitleTextBlock/Text").c_str());

        HICON ApplicationIconHandle = reinterpret_cast<HICON>(::LoadImageW(
            ::GetModuleHandleW(nullptr),
            MAKEINTRESOURCE(IDI_ICON),
            IMAGE_ICON,
            256,
            256,
            LR_SHARED));
        if (ApplicationIconHandle)
        {
            ::SendMessageW(
                this->m_WindowHandle,
                WM_SETICON,
                TRUE,
                reinterpret_cast<LPARAM>(ApplicationIconHandle));
            ::SendMessageW(
                this->m_WindowHandle,
                WM_SETICON,
                FALSE,
                reinterpret_cast<LPARAM>(ApplicationIconHandle));
        }
    }

    void SponsorPage::InitializeComponent()
    {
        SponsorPageT::InitializeComponent();
    }

    void SponsorPage::ContributeButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = { 0 };
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile =
            L"https://github.com/M2Team/NanaZip/"
            L"blob/main/CONTRIBUTING.md";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }

    winrt::fire_and_forget SponsorPage::BuySponsorEditionButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        winrt::StoreContext Context = winrt::StoreContext::GetDefault();
        if (Context)
        {
            winrt::check_hresult(
                Context.as<IInitializeWithWindow>()->Initialize(
                    this->m_WindowHandle));

            winrt::StoreProductQueryResult ProductQueryResult =
                co_await Context.GetStoreProductsAsync(
                    { L"Durable" },
                    { L"9N9DNPT6D6Z9" });
            for (auto Item : ProductQueryResult.Products())
            {
                winrt::StoreProduct Product = Item.Value();

                co_await Product.RequestPurchaseAsync();
            }
        }
    }

    void SponsorPage::SponsorEditionPolicyButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        SHELLEXECUTEINFOW ExecInfo = { 0 };
        ExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        ExecInfo.lpVerb = L"open";
        ExecInfo.lpFile =
            L"https://github.com/M2Team/NanaZip/"
            L"blob/main/Documents/SponsorEdition.md";
        ExecInfo.nShow = SW_SHOWNORMAL;
        ::ShellExecuteExW(&ExecInfo);
    }
}

EXTERN_C LPVOID WINAPI K7ModernCreateSponsorPage(
    _In_ HWND ParentWindowHandle)
{
    using Interface =
        winrt::NanaZip::ModernExperience::SponsorPage;
    using Implementation =
        winrt::NanaZip::ModernExperience::implementation::SponsorPage;

    Interface Window = winrt::make<Implementation>(ParentWindowHandle);
    return winrt::detach_abi(Window);
}
