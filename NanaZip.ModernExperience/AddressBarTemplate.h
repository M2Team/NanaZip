#pragma once

#include "AddressBarTemplate.g.h"

namespace winrt::NanaZip::ModernExperience::implementation
{
    struct AddressBarTemplate : AddressBarTemplateT<AddressBarTemplate>
    {
        AddressBarTemplate()
        {

        }
    };
}

namespace winrt::NanaZip::ModernExperience::factory_implementation
{
    struct AddressBarTemplate : AddressBarTemplateT<AddressBarTemplate, implementation::AddressBarTemplate>
    { };
}
