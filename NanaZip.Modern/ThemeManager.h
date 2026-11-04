/*
 * PROJECT:    NanaZip
 * FILE:       ThemeManager.h
 * PURPOSE:    Theme management for NanaZip Modern UI
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_MODERN_THEME_MANAGER
#define NANAZIP_MODERN_THEME_MANAGER

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::Storage::ApplicationDataContainer;
    using Windows::UI::Xaml::Controls::Page;
    using Windows::UI::Xaml::ResourceDictionary;
    using Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
}

namespace NanaZip::Modern
{
    enum class AppTheme : int32_t
    {
        Transparent = 0,
        Glassy = 1,
        Matte = 2
    };

    struct ThemeManager
    {
        // Singleton instance
        static ThemeManager& Instance();

        // Initialize theme manager
        void Initialize();

        // Get current theme
        AppTheme CurrentTheme() const;

        // Set theme and persist
        void SetTheme(AppTheme theme);

        // Apply theme to a XAML source (window)
        void ApplyThemeToWindow(winrt::DesktopWindowXamlSource const& xamlSource);

        // Apply theme to a Page
        void ApplyThemeToPage(winrt::Page const& page);

        // Get system backdrop type for current theme
        int32_t GetSystemBackdropType() const;

        // Theme changed event
        winrt::event_token ThemeChanged(winrt::Windows::Foundation::EventHandler<IInspectable> const& handler);
        void ThemeChanged(winrt::event_token const& token);

    private:
        ThemeManager() = default;
        ~ThemeManager() = default;
        ThemeManager(ThemeManager const&) = delete;
        ThemeManager& operator=(ThemeManager const&) = delete;

        void LoadThemeFromSettings();
        void SaveThemeToSettings(AppTheme theme);
        void LoadThemeResources(AppTheme theme);
        void ApplyThemeResources(winrt::ResourceDictionary const& targetResources);

        AppTheme m_currentTheme = AppTheme::Glassy; // Default to Glassy
        winrt::ResourceDictionary m_themeResources = nullptr;
        winrt::event<winrt::Windows::Foundation::EventHandler<IInspectable>> m_themeChangedEvent;
    };
}

#endif // NANAZIP_MODERN_THEME_MANAGER