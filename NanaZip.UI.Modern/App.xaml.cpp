#include "pch.h"

#include "App.h"

#include <Mile.Xaml.h>

namespace winrt::NanaZip::Modern::implementation
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
