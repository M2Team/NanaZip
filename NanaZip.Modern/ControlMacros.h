#pragma once

#define DEPENDENCY_PROPERTY_HEADER(name, type) \
    type name(); \
    void name(type const&); \
    static ::winrt::Windows::UI::Xaml::DependencyProperty \
        name##Property();

#define DEPENDENCY_PROPERTY_SOURCE_NOBOX(name, type, implType, projType) \
    ::winrt::Windows::UI::Xaml::DependencyProperty s_##name##Property{ nullptr }; \
    type implType::name() \
    { \
        return GetValue(name##Property()).as<type>(); \
    } \
    \
    void implType::name(type const& name) \
    { \
        SetValue(name##Property(), name); \
    } \
    ::winrt::Windows::UI::Xaml::DependencyProperty \
        implType::name##Property() \
    { \
        if (!s_##name##Property) \
        { \
            s_##name##Property = \
                ::winrt::Windows::UI::Xaml::DependencyProperty::Register( \
                    L#name, \
                    ::winrt::xaml_typename<type>(), \
                    ::winrt::xaml_typename<projType>(), \
                    nullptr); \
        } \
        return s_##name##Property; \
    }

#define DEPENDENCY_PROPERTY_SOURCE_BOX(name, type, implType, projType) \
    ::winrt::Windows::UI::Xaml::DependencyProperty s_##name##Property{ nullptr }; \
    type implType::name() \
    { \
        return ::winrt::unbox_value<type>(GetValue(name##Property())); \
    } \
    \
    void implType::name(type const& name) \
    { \
        SetValue(name##Property(), ::winrt::box_value(name)); \
    } \
    ::winrt::Windows::UI::Xaml::DependencyProperty \
        implType::name##Property() \
    { \
        if (!s_##name##Property) \
        { \
            s_##name##Property = \
                ::winrt::Windows::UI::Xaml::DependencyProperty::Register( \
                    L#name, \
                    ::winrt::xaml_typename<type>(), \
                    ::winrt::xaml_typename<projType>(), \
                    nullptr); \
        } \
        return s_##name##Property; \
    }

#define DEPENDENCY_PROPERTY_SOURCE_BOX_WITHDEFAULT(name, type, implType, projType, default) \
    ::winrt::Windows::UI::Xaml::DependencyProperty s_##name##Property{ nullptr }; \
    type implType::name() \
    { \
        return ::winrt::unbox_value<type>(GetValue(name##Property())); \
    } \
    \
    void implType::name(type const& name) \
    { \
        SetValue(name##Property(), ::winrt::box_value(name)); \
    } \
    ::winrt::Windows::UI::Xaml::DependencyProperty \
        implType::name##Property() \
    { \
        if (!s_##name##Property) \
        { \
            s_##name##Property = \
                ::winrt::Windows::UI::Xaml::DependencyProperty::Register( \
                    L#name, \
                    ::winrt::xaml_typename<type>(), \
                    ::winrt::xaml_typename<projType>(), \
                    ::winrt::Windows::UI::Xaml::PropertyMetadata( \
                        ::winrt::box_value(default), nullptr \
                    ) \
                ); \
        } \
        return s_##name##Property; \
    }
