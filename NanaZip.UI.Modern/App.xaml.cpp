#include "pch.h"

#include "App.h"

#include <Mile.Xaml.h>

namespace winrt::NanaZip::Modern::implementation
{
    App::App()
    {
        ::MileXamlGlobalInitialize();
    }

    void App::Close()
    {
        Exit();
        ::MileXamlGlobalUninitialize();
    }
}
