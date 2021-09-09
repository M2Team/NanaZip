// Windows/Net.cpp

#include "StdAfx.h"

#include "../Common/MyBuffer.h"

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif

#include "Net.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NNet {

DWORD CEnum::Open(DWORD scope, DWORD type, DWORD usage, LPNETRESOURCE netResource)
{
  Close();
  DWORD result = ::WNetOpenEnum(scope, type, usage, netResource, &_handle);
  _handleAllocated = (result == NO_ERROR);
  return result;
}

#ifndef _UNICODE
DWORD CEnum::Open(DWORD scope, DWORD type, DWORD usage, LPNETRESOURCEW netResource)
{
  Close();
  DWORD result = ::WNetOpenEnumW(scope, type, usage, netResource, &_handle);
  _handleAllocated = (result == NO_ERROR);
  return result;
}
#endif

static void SetComplexString(bool &defined, CSysString &destString, LPCTSTR srcString)
{
  defined = (srcString != 0);
  if (defined)
    destString = srcString;
  else
    destString.Empty();
}

static void ConvertNETRESOURCEToCResource(const NETRESOURCE &netResource, CResource &resource)
{
  resource.Scope = netResource.dwScope;
  resource.Type = netResource.dwType;
  resource.DisplayType = netResource.dwDisplayType;
  resource.Usage = netResource.dwUsage;
  SetComplexString(resource.LocalNameIsDefined, resource.LocalName, netResource.lpLocalName);
  SetComplexString(resource.RemoteNameIsDefined, resource.RemoteName, netResource.lpRemoteName);
  SetComplexString(resource.CommentIsDefined, resource.Comment, netResource.lpComment);
  SetComplexString(resource.ProviderIsDefined, resource.Provider, netResource.lpProvider);
}

static void SetComplexString2(LPTSTR *destString, bool defined, const CSysString &srcString)
{
  if (defined)
    *destString = srcString.Ptr_non_const();
  else
    *destString = NULL;
}

static void ConvertCResourceToNETRESOURCE(const CResource &resource, NETRESOURCE &netResource)
{
  netResource.dwScope = resource.Scope;
  netResource.dwType = resource.Type;
  netResource.dwDisplayType = resource.DisplayType;
  netResource.dwUsage = resource.Usage;
  SetComplexString2(&netResource.lpLocalName, resource.LocalNameIsDefined, resource.LocalName);
  SetComplexString2(&netResource.lpRemoteName, resource.RemoteNameIsDefined, resource.RemoteName);
  SetComplexString2(&netResource.lpComment, resource.CommentIsDefined, resource.Comment);
  SetComplexString2(&netResource.lpProvider, resource.ProviderIsDefined, resource.Provider);
}

#ifndef _UNICODE

static void SetComplexString(bool &defined, UString &destString, LPCWSTR src)
{
  defined = (src != NULL);
  if (defined)
    destString = src;
  else
    destString.Empty();
}

static void ConvertNETRESOURCEToCResource(const NETRESOURCEW &netResource, CResourceW &resource)
{
  resource.Scope = netResource.dwScope;
  resource.Type = netResource.dwType;
  resource.DisplayType = netResource.dwDisplayType;
  resource.Usage = netResource.dwUsage;
  SetComplexString(resource.LocalNameIsDefined, resource.LocalName, netResource.lpLocalName);
  SetComplexString(resource.RemoteNameIsDefined, resource.RemoteName, netResource.lpRemoteName);
  SetComplexString(resource.CommentIsDefined, resource.Comment, netResource.lpComment);
  SetComplexString(resource.ProviderIsDefined, resource.Provider, netResource.lpProvider);
}

static void SetComplexString2(LPWSTR *destString, bool defined, const UString &srcString)
{
  if (defined)
    *destString = srcString.Ptr_non_const();
  else
    *destString = NULL;
}

static void ConvertCResourceToNETRESOURCE(const CResourceW &resource, NETRESOURCEW &netResource)
{
  netResource.dwScope = resource.Scope;
  netResource.dwType = resource.Type;
  netResource.dwDisplayType = resource.DisplayType;
  netResource.dwUsage = resource.Usage;
  SetComplexString2(&netResource.lpLocalName, resource.LocalNameIsDefined, resource.LocalName);
  SetComplexString2(&netResource.lpRemoteName, resource.RemoteNameIsDefined, resource.RemoteName);
  SetComplexString2(&netResource.lpComment, resource.CommentIsDefined, resource.Comment);
  SetComplexString2(&netResource.lpProvider, resource.ProviderIsDefined, resource.Provider);
}

static void ConvertResourceWToResource(const CResourceW &resourceW, CResource &resource)
{
  *(CResourceBase *)&resource = *(CResourceBase *)&resourceW;
  resource.LocalName = GetSystemString(resourceW.LocalName);
  resource.RemoteName = GetSystemString(resourceW.RemoteName);
  resource.Comment = GetSystemString(resourceW.Comment);
  resource.Provider = GetSystemString(resourceW.Provider);
}

static void ConvertResourceToResourceW(const CResource &resource, CResourceW &resourceW)
{
  *(CResourceBase *)&resourceW = *(CResourceBase *)&resource;
  resourceW.LocalName = GetUnicodeString(resource.LocalName);
  resourceW.RemoteName = GetUnicodeString(resource.RemoteName);
  resourceW.Comment = GetUnicodeString(resource.Comment);
  resourceW.Provider = GetUnicodeString(resource.Provider);
}
#endif

DWORD CEnum::Open(DWORD scope, DWORD type, DWORD usage, const CResource *resource)
{
  NETRESOURCE netResource;
  LPNETRESOURCE pointer = NULL;
  if (resource)
  {
    ConvertCResourceToNETRESOURCE(*resource, netResource);
    pointer = &netResource;
  }
  return Open(scope, type, usage, pointer);
}

#ifndef _UNICODE
DWORD CEnum::Open(DWORD scope, DWORD type, DWORD usage, const CResourceW *resource)
{
  if (g_IsNT)
  {
    NETRESOURCEW netResource;
    LPNETRESOURCEW pointer = NULL;
    if (resource)
    {
      ConvertCResourceToNETRESOURCE(*resource, netResource);
      pointer = &netResource;
    }
    return Open(scope, type, usage, pointer);
  }
  CResource resourceA;
  CResource *pointer = NULL;
  if (resource)
  {
    ConvertResourceWToResource(*resource, resourceA);
    pointer = &resourceA;
  }
  return Open(scope, type, usage, pointer);
}
#endif

DWORD CEnum::Close()
{
  if (!_handleAllocated)
    return NO_ERROR;
  DWORD result = ::WNetCloseEnum(_handle);
  _handleAllocated = (result != NO_ERROR);
  return result;
}

DWORD CEnum::Next(LPDWORD lpcCount, LPVOID lpBuffer, LPDWORD lpBufferSize)
{
  return ::WNetEnumResource(_handle, lpcCount, lpBuffer, lpBufferSize);
}

#ifndef _UNICODE
DWORD CEnum::NextW(LPDWORD lpcCount, LPVOID lpBuffer, LPDWORD lpBufferSize)
{
  return ::WNetEnumResourceW(_handle, lpcCount, lpBuffer, lpBufferSize);
}
#endif

DWORD CEnum::Next(CResource &resource)
{
  const DWORD kBufferSize = 16384;
  CByteArr byteBuffer(kBufferSize);
  LPNETRESOURCE lpnrLocal = (LPNETRESOURCE) (void *) (BYTE *)(byteBuffer);
  ZeroMemory(lpnrLocal, kBufferSize);
  DWORD bufferSize = kBufferSize;
  DWORD numEntries = 1;
  DWORD result = Next(&numEntries, lpnrLocal, &bufferSize);
  if (result != NO_ERROR)
    return result;
  if (numEntries != 1)
    return (DWORD)E_FAIL;
  ConvertNETRESOURCEToCResource(lpnrLocal[0], resource);
  return result;
}

#ifndef _UNICODE
DWORD CEnum::Next(CResourceW &resource)
{
  if (g_IsNT)
  {
    const DWORD kBufferSize = 16384;
    CByteArr byteBuffer(kBufferSize);
    LPNETRESOURCEW lpnrLocal = (LPNETRESOURCEW) (void *) (BYTE *)(byteBuffer);
    ZeroMemory(lpnrLocal, kBufferSize);
    DWORD bufferSize = kBufferSize;
    DWORD numEntries = 1;
    DWORD result = NextW(&numEntries, lpnrLocal, &bufferSize);
    if (result != NO_ERROR)
      return result;
    if (numEntries != 1)
      return (DWORD)E_FAIL;
    ConvertNETRESOURCEToCResource(lpnrLocal[0], resource);
    return result;
  }
  CResource resourceA;
  DWORD result = Next(resourceA);
  ConvertResourceToResourceW(resourceA, resource);
  return result;
}
#endif


DWORD GetResourceParent(const CResource &resource, CResource &parentResource)
{
  const DWORD kBufferSize = 16384;
  CByteArr byteBuffer(kBufferSize);
  LPNETRESOURCE lpnrLocal = (LPNETRESOURCE) (void *) (BYTE *)(byteBuffer);
  ZeroMemory(lpnrLocal, kBufferSize);
  DWORD bufferSize = kBufferSize;
  NETRESOURCE netResource;
  ConvertCResourceToNETRESOURCE(resource, netResource);
  DWORD result = ::WNetGetResourceParent(&netResource, lpnrLocal, &bufferSize);
  if (result != NO_ERROR)
    return result;
  ConvertNETRESOURCEToCResource(lpnrLocal[0], parentResource);
  return result;
}

#ifndef _UNICODE
DWORD GetResourceParent(const CResourceW &resource, CResourceW &parentResource)
{
  if (g_IsNT)
  {
    const DWORD kBufferSize = 16384;
    CByteArr byteBuffer(kBufferSize);
    LPNETRESOURCEW lpnrLocal = (LPNETRESOURCEW) (void *) (BYTE *)(byteBuffer);
    ZeroMemory(lpnrLocal, kBufferSize);
    DWORD bufferSize = kBufferSize;
    NETRESOURCEW netResource;
    ConvertCResourceToNETRESOURCE(resource, netResource);
    DWORD result = ::WNetGetResourceParentW(&netResource, lpnrLocal, &bufferSize);
    if (result != NO_ERROR)
      return result;
    ConvertNETRESOURCEToCResource(lpnrLocal[0], parentResource);
    return result;
  }
  CResource resourceA, parentResourceA;
  ConvertResourceWToResource(resource, resourceA);
  DWORD result = GetResourceParent(resourceA, parentResourceA);
  ConvertResourceToResourceW(parentResourceA, parentResource);
  return result;
}
#endif

DWORD GetResourceInformation(const CResource &resource,
    CResource &destResource, CSysString &systemPathPart)
{
  const DWORD kBufferSize = 16384;
  CByteArr byteBuffer(kBufferSize);
  LPNETRESOURCE lpnrLocal = (LPNETRESOURCE) (void *) (BYTE *)(byteBuffer);
  ZeroMemory(lpnrLocal, kBufferSize);
  DWORD bufferSize = kBufferSize;
  NETRESOURCE netResource;
  ConvertCResourceToNETRESOURCE(resource, netResource);
  LPTSTR lplpSystem;
  DWORD result = ::WNetGetResourceInformation(&netResource,
      lpnrLocal, &bufferSize, &lplpSystem);
  if (result != NO_ERROR)
    return result;
  if (lplpSystem != 0)
    systemPathPart = lplpSystem;
  ConvertNETRESOURCEToCResource(lpnrLocal[0], destResource);
  return result;
}

#ifndef _UNICODE
DWORD GetResourceInformation(const CResourceW &resource,
    CResourceW &destResource, UString &systemPathPart)
{
  if (g_IsNT)
  {
    const DWORD kBufferSize = 16384;
    CByteArr byteBuffer(kBufferSize);
    LPNETRESOURCEW lpnrLocal = (LPNETRESOURCEW) (void *) (BYTE *)(byteBuffer);
    ZeroMemory(lpnrLocal, kBufferSize);
    DWORD bufferSize = kBufferSize;
    NETRESOURCEW netResource;
    ConvertCResourceToNETRESOURCE(resource, netResource);
    LPWSTR lplpSystem;
    DWORD result = ::WNetGetResourceInformationW(&netResource,
      lpnrLocal, &bufferSize, &lplpSystem);
    if (result != NO_ERROR)
      return result;
    if (lplpSystem != 0)
      systemPathPart = lplpSystem;
    ConvertNETRESOURCEToCResource(lpnrLocal[0], destResource);
    return result;
  }
  CResource resourceA, destResourceA;
  ConvertResourceWToResource(resource, resourceA);
  AString systemPathPartA;
  DWORD result = GetResourceInformation(resourceA, destResourceA, systemPathPartA);
  ConvertResourceToResourceW(destResourceA, destResource);
  systemPathPart = GetUnicodeString(systemPathPartA);
  return result;
}
#endif

DWORD AddConnection2(const CResource &resource,
    LPCTSTR password, LPCTSTR userName, DWORD flags)
{
  NETRESOURCE netResource;
  ConvertCResourceToNETRESOURCE(resource, netResource);
  return ::WNetAddConnection2(&netResource,
    password, userName, flags);
}

DWORD AddConnection2(const CResource &resource, LPCTSTR password, LPCTSTR userName, DWORD flags);

#ifndef _UNICODE
DWORD AddConnection2(const CResourceW &resource, LPCWSTR password, LPCWSTR userName, DWORD flags)
{
  if (g_IsNT)
  {
    NETRESOURCEW netResource;
    ConvertCResourceToNETRESOURCE(resource, netResource);
    return ::WNetAddConnection2W(&netResource,password, userName, flags);
  }
  CResource resourceA;
  ConvertResourceWToResource(resource, resourceA);
  const CSysString passwordA (GetSystemString(password));
  const CSysString userNameA (GetSystemString(userName));
  return AddConnection2(resourceA,
    password ? (LPCTSTR)passwordA: 0,
    userName ? (LPCTSTR)userNameA: 0,
    flags);
}
#endif

}}
