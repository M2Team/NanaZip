#include "pch.h"

#include "App.h"
#if __has_include("App.g.cpp")
#include "App.g.cpp"
#endif

#include <Mile.Xaml.h>

namespace winrt::NanaZip::ModernExperience::implementation
{
    App::App()
    {
        // Workaround for unhandled exception at twinapi.appcore.dll
        // Fixes: https://github.com/M2Team/NanaZip/issues/400
        this->AddRef();
        ::MileXamlGlobalInitialize();
    }

    void App::Close()
    {
        Exit();
        ::MileXamlGlobalUninitialize();
    }
}
