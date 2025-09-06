#include "Restrictions.h"

#include <cstdlib>
#include <cstring>

static PCZZSTR NanaZipPoliciesGetMultiStringA(PCSTR Name)
{
    PZZSTR Result = nullptr;
    DWORD Length = 0;
    LSTATUS Status;

    ::RegGetValueA(
        HKEY_LOCAL_MACHINE,
        "Software\\NanaZip\\Policies",
        Name,
        RRF_RT_REG_MULTI_SZ,
        nullptr,
        nullptr,
        &Length);
    if (Length == 0)
    {
        return nullptr;
    }

    Result = static_cast<PZZSTR>(std::calloc(Length, 1));
    if (!Result)
    {
        return nullptr;
    }

    Status = ::RegGetValueA(
        HKEY_LOCAL_MACHINE,
        "Software\\NanaZip\\Policies",
        Name,
        RRF_RT_REG_MULTI_SZ,
        nullptr,
        reinterpret_cast<PVOID>(Result),
        &Length);
    if (Status != ERROR_SUCCESS)
    {
        std::free(Result);
        return nullptr;
    }

    return Result;
}

static bool NanaZipMultiStringFindStringA(PCZZSTR Haystack, PCSTR Needle)
{
    PCSTR Current;

    if (!Haystack || !Needle)
    {
        return false;
    }

    for (Current = Haystack;
        *Current;
        Current = Current + std::strlen(Current) + 1)
    {
        if (!std::strcmp(Current, Needle))
        {
            return true;
        }
    }

    return false;
}

static PCZZSTR NanaZipGetAllowedHandlersA()
{
    static PCZZSTR CachedResult = ::NanaZipPoliciesGetMultiStringA(
        "AllowedHandlers");
    return CachedResult;
}

static PCZZSTR NanaZipGetBlockedHandlersA()
{
    static PCZZSTR CachedResult = ::NanaZipPoliciesGetMultiStringA(
        "BlockedHandlers");
    return CachedResult;
}

static PCZZSTR NanaZipGetAllowedCodecsA()
{
    static PCZZSTR CachedResult = ::NanaZipPoliciesGetMultiStringA(
        "AllowedCodecs");
    return CachedResult;
}

static PCZZSTR NanaZipGetBlockedCodecsA()
{
    static PCZZSTR CachedResult = ::NanaZipPoliciesGetMultiStringA(
        "BlockedCodecs");
    return CachedResult;
}

static bool NanaZipIsElementAllowedA(PCSTR Allowed, PCSTR Blocked, PCSTR Element)
{
    if (Allowed && !::NanaZipMultiStringFindStringA(Allowed, Element))
    {
        return false;
    }
    if (Blocked && ::NanaZipMultiStringFindStringA(Blocked, Element))
    {
        return false;
    }
    return true;
}

EXTERN_C bool NanaZipIsHandlerAllowedA(PCSTR HandlerName)
{
    return NanaZipIsElementAllowedA(
        NanaZipGetAllowedHandlersA(),
        NanaZipGetBlockedHandlersA(),
        HandlerName);
}

EXTERN_C bool NanaZipIsCodecAllowedA(PCSTR CodecName)
{
    return NanaZipIsElementAllowedA(
        NanaZipGetAllowedCodecsA(),
        NanaZipGetBlockedCodecsA(),
        CodecName);
}
