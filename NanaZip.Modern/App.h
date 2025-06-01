#pragma once

#include "App.g.h"

namespace winrt
{
    using Windows::UI::Xaml::Interop::TypeName;
    using Windows::UI::Xaml::Markup::IXamlMetadataProvider;
    using Windows::UI::Xaml::Markup::IXamlType;
    using Windows::UI::Xaml::Markup::XmlnsDefinition;
}

namespace winrt::NanaZip::Modern::implementation
{
    template <typename D, typename ... Interfaces>
    struct AppT2 : public App_base<D, winrt::IXamlMetadataProvider, Interfaces...>
    {
        winrt::IXamlType GetXamlType(
            winrt::TypeName const& type)
        {
            return AppProvider()->GetXamlType(type);
        }

        winrt::IXamlType GetXamlType(
            winrt::hstring const& fullName)
        {
            return AppProvider()->GetXamlType(fullName);
        }

        winrt::com_array<winrt::XmlnsDefinition> GetXmlnsDefinitions()
        {
            return AppProvider()->GetXmlnsDefinitions();
        }

    private:

        bool _contentLoaded{ false };

        winrt::com_ptr<XamlMetaDataProvider> _appProvider;

        winrt::com_ptr<XamlMetaDataProvider> AppProvider()
        {
            if (!_appProvider)
            {
                _appProvider = winrt::make_self<XamlMetaDataProvider>();
            }
            return _appProvider;
        }
    };

    struct App : AppT2<App>
    {
    public:
        App();
        void Close();
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
