#pragma once

#include "AboutPage.g.h"

namespace winrt::NanaZip::implementation
{
    struct AboutPage : AboutPageT<AboutPage>
    {
        AboutPage();
    };
}

namespace winrt::NanaZip::factory_implementation
{
    struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
