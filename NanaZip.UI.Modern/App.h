#pragma once
#include "App.xaml.g.h"

namespace winrt::NanaZip::Modern::implementation
{
    class App : public AppT<App>
    {
    public:
        App();
        void Close();
    };
}
