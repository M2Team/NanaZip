#pragma once

#include "StatusBar.g.h"

namespace wux = ::winrt::Windows::UI::Xaml;
namespace wuxc = wux::Controls;

namespace winrt::NanaZip::ModernExperience::implementation
{
    struct StatusBar : StatusBarT<StatusBar>
    {
        StatusBar() = default;

        winrt::hstring Text1() const&;
        void Text1(winrt::hstring const&);

        winrt::hstring Text2() const&;
        void Text2(winrt::hstring const&);

        winrt::hstring Text3() const&;
        void Text3(winrt::hstring const&);

        winrt::hstring Text4() const&;
        void Text4(winrt::hstring const&);

        static wux::DependencyProperty Text1Property();
        static wux::DependencyProperty Text2Property();
        static wux::DependencyProperty Text3Property();
        static wux::DependencyProperty Text4Property();

    private:
        inline static wux::DependencyProperty s_text1Property{ nullptr };
        inline static wux::DependencyProperty s_text2Property{ nullptr };
        inline static wux::DependencyProperty s_text3Property{ nullptr };
        inline static wux::DependencyProperty s_text4Property{ nullptr };
    };
}

namespace winrt::NanaZip::ModernExperience::factory_implementation
{
    struct StatusBar : StatusBarT<StatusBar, implementation::StatusBar>
    { };
}
