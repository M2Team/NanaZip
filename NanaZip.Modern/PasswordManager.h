#pragma once

#include <Windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>

namespace winrt
{
    using Windows::Foundation::IAsyncOperation;
    using Windows::Security::Credentials::UI::UserConsentVerifier;
    using Windows::Security::Credentials::UI::UserConsentVerificationResult;
}

namespace NanaZip::Modern::PasswordManager
{
    winrt::IAsyncOperation<winrt::UserConsentVerificationResult> WindowsHelloVerifier(
        _In_ HWND ParentWindow);
}
