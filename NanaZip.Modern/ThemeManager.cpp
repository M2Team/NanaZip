/*
 * PROJECT:    NanaZip
 * FILE:       ThemeManager.cpp
 * PURPOSE:    Theme management implementation for NanaZip Modern UI
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "pch.h"
#include "ThemeManager.h"

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::Storage::ApplicationData;
    using Windows::Storage::ApplicationDataContainer;
    using Windows::UI::Xaml::Controls::Page;
    using Windows::UI::Xaml::ResourceDictionary;
    using Windows::UI::Xaml::Markup::XamlReader;
    using Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
}

namespace NanaZip::Modern
{
    // MILE_WINDOW_SYSTEM_BACKDROP_TYPE constants (from Mile.Xaml)
    constexpr int32_t MILE_WINDOW_SYSTEM_BACKDROP_TYPE_AUTO = 0;
    constexpr int32_t MILE_WINDOW_SYSTEM_BACKDROP_TYPE_NONE = 1;
    constexpr int32_t MILE_WINDOW_SYSTEM_BACKDROP_TYPE_MICA = 2;
    constexpr int32_t MILE_WINDOW_SYSTEM_BACKDROP_TYPE_ACRYLIC = 3;
    constexpr int32_t MILE_WINDOW_SYSTEM_BACKDROP_TYPE_TRANSPARENT = 4;

    ThemeManager& ThemeManager::Instance()
    {
        static ThemeManager instance;
        return instance;
    }

    void ThemeManager::Initialize()
    {
        LoadThemeFromSettings();
        LoadThemeResources(m_currentTheme);
    }

    AppTheme ThemeManager::CurrentTheme() const
    {
        return m_currentTheme;
    }

    void ThemeManager::SetTheme(AppTheme theme)
    {
        if (m_currentTheme != theme)
        {
            m_currentTheme = theme;
            LoadThemeResources(theme);
            SaveThemeToSettings(theme);
            m_themeChangedEvent(*this, nullptr);
        }
    }

    void ThemeManager::ApplyThemeToWindow(winrt::DesktopWindowXamlSource const& xamlSource)
    {
        if (!xamlSource || !m_themeResources)
        {
            return;
        }

        try
        {
            // Get the root visual and apply theme resources
            auto content = xamlSource.Content();
            if (content)
            {
                auto frameworkElement = content.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
                if (frameworkElement)
                {
                    ApplyThemeResources(frameworkElement.Resources());
                }
            }
        }
        catch (...)
        {
            // Ignore errors
        }
    }

    void ThemeManager::ApplyThemeToPage(winrt::Page const& page)
    {
        if (!page || !m_themeResources)
        {
            return;
        }

        try
        {
            ApplyThemeResources(page.Resources());
        }
        catch (...)
        {
            // Ignore errors
        }
    }

    int32_t ThemeManager::GetSystemBackdropType() const
    {
        switch (m_currentTheme)
        {
        case AppTheme::Transparent:
            return MILE_WINDOW_SYSTEM_BACKDROP_TYPE_TRANSPARENT;
        case AppTheme::Glassy:
            return MILE_WINDOW_SYSTEM_BACKDROP_TYPE_MICA;
        case AppTheme::Matte:
            return MILE_WINDOW_SYSTEM_BACKDROP_TYPE_ACRYLIC;
        default:
            return MILE_WINDOW_SYSTEM_BACKDROP_TYPE_AUTO;
        }
    }

    winrt::event_token ThemeManager::ThemeChanged(winrt::Windows::Foundation::EventHandler<IInspectable> const& handler)
    {
        return m_themeChangedEvent.add(handler);
    }

    void ThemeManager::ThemeChanged(winrt::event_token const& token)
    {
        m_themeChangedEvent.remove(token);
    }

    void ThemeManager::LoadThemeFromSettings()
    {
        try
        {
            auto localSettings = ApplicationData::Current().LocalSettings();
            auto values = localSettings.Values();

            if (values.HasKey(L"AppTheme"))
            {
                auto themeValue = values.Lookup(L"AppTheme");
                if (themeValue)
                {
                    int32_t themeInt = winrt::unbox_value<int32_t>(themeValue);
                    if (themeInt >= 0 && themeInt <= 2)
                    {
                        m_currentTheme = static_cast<AppTheme>(themeInt);
                    }
                }
            }
        }
        catch (...)
        {
            // Use default theme (Glassy)
            m_currentTheme = AppTheme::Glassy;
        }
    }

    void ThemeManager::SaveThemeToSettings(AppTheme theme)
    {
        try
        {
            auto localSettings = ApplicationData::Current().LocalSettings();
            auto values = localSettings.Values();
            values.Insert(L"AppTheme", winrt::box_value(static_cast<int32_t>(theme)));
        }
        catch (...)
        {
            // Ignore errors
        }
    }

    void ThemeManager::LoadThemeResources(AppTheme theme)
    {
        try
        {
            winrt::hstring xamlContent;

            switch (theme)
            {
            case AppTheme::Transparent:
                xamlContent = LR"(<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Source="ms-appx:///NanaZip.Modern/Themes/TransparentTheme.xaml" />)";
                break;
            case AppTheme::Glassy:
                xamlContent = LR"(<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Source="ms-appx:///NanaZip.Modern/Themes/GlassyTheme.xaml" />)";
                break;
            case AppTheme::Matte:
                xamlContent = LR"(<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Source="ms-appx:///NanaZip.Modern/Themes/MatteTheme.xaml" />)";
                break;
            default:
                xamlContent = LR"(<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Source="ms-appx:///NanaZip.Modern/Themes/GlassyTheme.xaml" />)";
                break;
            }

            m_themeResources = XamlReader::Load(xamlContent).as<ResourceDictionary>();
        }
        catch (...)
        {
            // Fallback to Glassy theme
            try
            {
                winrt::hstring xamlContent = LR"(<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    Source="ms-appx:///NanaZip.Modern/Themes/GlassyTheme.xaml" />)";
                m_themeResources = XamlReader::Load(xamlContent).as<ResourceDictionary>();
                m_currentTheme = AppTheme::Glassy;
            }
            catch (...)
            {
                m_themeResources = nullptr;
            }
        }
    }

    void ThemeManager::ApplyThemeResources(winrt::ResourceDictionary const& targetResources)
    {
        if (!targetResources || !m_themeResources)
        {
            return;
        }

        try
        {
            // Merge theme resources into target
            targetResources.MergedDictionaries().Clear();

            // First add the base theme resources
            auto baseResources = winrt::make<ResourceDictionary>();
            baseResources.Source(winrt::Windows::Foundation::Uri(L"ms-appx:///NanaZip.Modern/Themes/ThemeResources.xaml"));
            targetResources.MergedDictionaries().Append(baseResources);

            // Then add the selected theme
            targetResources.MergedDictionaries().Append(m_themeResources);
        }
        catch (...)
        {
            // Ignore errors
        }
    }
}