#ifndef __COPY_HOOK_H
#define __COPY_HOOK_H

#include "../../../Common/MyWindows.h"

#define COPYHOOK_UUID "{7F4FD2EA-8CC8-43C4-8440-CD76805B4E95}"
#define COPYHOOK_COPY 0x7F4FD2EA

struct CopyHookData {
    wchar_t filename[MAX_PATH];
};

#endif
