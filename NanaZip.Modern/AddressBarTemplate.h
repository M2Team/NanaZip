#pragma once

#include "AddressBarTemplate.g.h"

namespace winrt::NanaZip::Modern::implementation
{
    struct AddressBarTemplate : AddressBarTemplateT<AddressBarTemplate>
    {
        AddressBarTemplate()
        {

        }
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct AddressBarTemplate : AddressBarTemplateT<AddressBarTemplate, implementation::AddressBarTemplate>
    { };
}
