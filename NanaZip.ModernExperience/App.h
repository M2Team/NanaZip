#pragma once
#include "App.xaml.g.h"

namespace winrt::NanaZip::ModernExperience::implementation
{
    class App : public AppT<App>
    {
    public:
        App();
        void Close();
    };
}
