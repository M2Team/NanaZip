#ifndef COPY_HOOK_H
#define COPY_HOOK_H

#include <Windows.h>

#define COPY_HOOK_PREFIX "{7F4FD2EA-8CC8-43C4-8440-CD76805B4E95}"
#define COPY_HOOK_COMMAND_COPY 0x7F4FD2EA

typedef struct _COPY_HOOK_DATA {
    wchar_t FileName[MAX_PATH];
} COPY_HOOK_DATA, *PCOPY_HOOK_DATA;

#endif
