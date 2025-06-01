#include "pch.h"
#include "StatusBar.h"
#include "StatusBar.g.cpp"

using namespace winrt::NanaZip::Modern::implementation;

winrt::hstring StatusBar::Text1() const&
{
    return winrt::unbox_value<winrt::hstring>(
        GetValue(Text1Property())
    );
}

void StatusBar::Text1(winrt::hstring const& text)
{
    SetValue(Text1Property(), winrt::box_value(text));
}

winrt::hstring StatusBar::Text2() const&
{
    return winrt::unbox_value<winrt::hstring>(
        GetValue(Text2Property())
    );
}

void StatusBar::Text2(winrt::hstring const& text)
{
    SetValue(Text2Property(), winrt::box_value(text));
}

winrt::hstring StatusBar::Text3() const&
{
    return winrt::unbox_value<winrt::hstring>(
        GetValue(Text3Property())
    );
}

void StatusBar::Text3(winrt::hstring const& text)
{
    SetValue(Text3Property(), winrt::box_value(text));
}

winrt::hstring StatusBar::Text4() const&
{
    return winrt::unbox_value<winrt::hstring>(
        GetValue(Text4Property())
    );
}

void StatusBar::Text4(winrt::hstring const& text)
{
    SetValue(Text4Property(), winrt::box_value(text));
}

wux::DependencyProperty StatusBar::Text1Property()
{
    if (!s_text1Property)
    {
        s_text1Property = wux::DependencyProperty::Register(
            L"Text1",
            winrt::xaml_typename<winrt::hstring>(),
            winrt::xaml_typename<winrt::NanaZip::Modern::StatusBar>(),
            nullptr
        );
    }
    return s_text1Property;
}

wux::DependencyProperty StatusBar::Text2Property()
{
    if (!s_text2Property)
    {
        s_text2Property = wux::DependencyProperty::Register(
            L"Text2",
            winrt::xaml_typename<winrt::hstring>(),
            winrt::xaml_typename<winrt::NanaZip::Modern::StatusBar>(),
            nullptr
        );
    }
    return s_text2Property;
}


wux::DependencyProperty StatusBar::Text3Property()
{
    if (!s_text3Property)
    {
        s_text3Property = wux::DependencyProperty::Register(
            L"Text3",
            winrt::xaml_typename<winrt::hstring>(),
            winrt::xaml_typename<winrt::NanaZip::Modern::StatusBar>(),
            nullptr
        );
    }
    return s_text3Property;
}


wux::DependencyProperty StatusBar::Text4Property()
{
    if (!s_text4Property)
    {
        s_text4Property = wux::DependencyProperty::Register(
            L"Text4",
            winrt::xaml_typename<winrt::hstring>(),
            winrt::xaml_typename<winrt::NanaZip::Modern::StatusBar>(),
            nullptr
        );
    }
    return s_text4Property;
}
