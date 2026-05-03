#include <PasswordManager.h>
#include <UserConsentVerifierInterop.h>

namespace NanaZip::Modern::PasswordManager
{
    winrt::IAsyncOperation<winrt::UserConsentVerificationResult> WindowsHelloVerifier(
        _In_ HWND ParentWindow)
    {
        // IUserConsentVerifierInterop has been public since Win10_RS3,
        // IUserConsentVerifierInterop has been documented since Win11_CO.
        winrt::hstring Message = L"[Use Windows Hello to get saved password.]";
        winrt::com_ptr<::IUserConsentVerifierInterop> Interop =
            winrt::get_activation_factory<winrt::UserConsentVerifier,
            ::IUserConsentVerifierInterop>();
        auto ConsentResult = co_await
            winrt::capture<
            winrt::IAsyncOperation<winrt::UserConsentVerificationResult>>(
            Interop,
            &::IUserConsentVerifierInterop::RequestVerificationForWindowAsync,
            ParentWindow,
            reinterpret_cast<HSTRING>(winrt::get_abi(Message)));
        co_return ConsentResult;
    }
}
