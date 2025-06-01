#pragma once

#include "StatusBarTemplate.g.h"

namespace winrt::NanaZip::Modern::implementation
{
    struct StatusBarTemplate : StatusBarTemplateT<StatusBarTemplate>
    {
        StatusBarTemplate()
        {
            // Xaml objects should not call InitializeComponent during construction.
            // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        }
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct StatusBarTemplate : StatusBarTemplateT<StatusBarTemplate, implementation::StatusBarTemplate>
    {
    };
}
