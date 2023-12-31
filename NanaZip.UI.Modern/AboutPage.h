#pragma once

#include "AboutPage.g.h"

namespace winrt::NanaZip::Modern::implementation
{
    struct AboutPage : AboutPageT<AboutPage>
    {
        AboutPage();
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
