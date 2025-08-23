#pragma once
#include <Windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// https://github.com/microsoft/terminal
// /blob/c727762602b8bd12e4a3a769053204d7e92b81c5
// /src/cascadia/WindowsTerminalUniversal/pch.h#L12
// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
// SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Navigation.h>
