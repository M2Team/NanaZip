// FileStreams.cpp

#include "StdAfx.h"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "../../Windows/FileFind.h"
#endif

#ifdef SUPPORT_DEVICE_FILE
#include "../../../C/Alloc.h"
#include "../../Common/Defs.h"
#endif

#include "FileStreams.h"

static inline HRESULT GetLastError_HRESULT()
{
  DWORD lastError = ::GetLastError();
  if (lastError == 0)
    return E_FAIL;
  return HRESULT_FROM_WIN32(lastError);
}

static inline HRESULT ConvertBoolToHRESULT(bool result)
{
  if (result)
    return S_OK;
  return GetLastError_HRESULT();
}


#ifdef SUPPORT_DEVICE_FILE
static const UInt32 kClusterSize = 1 << 18;
#endif

CInFileStream::CInFileStream():
  #ifdef SUPPORT_DEVICE_FILE
  VirtPos(0),
  PhyPos(0),
  Buf(0),
  BufSize(0),
  #endif
  SupportHardLinks(false),
  Callback(NULL),
  CallbackRef(0)
{
}

CInFileStream::~CInFileStream()
{
  #ifdef SUPPORT_DEVICE_FILE
  MidFree(Buf);
  #endif

  if (Callback)
    Callback->InFileStream_On_Destroy(CallbackRef);
}

STDMETHODIMP CInFileStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  #ifdef USE_WIN_FILE
  
  #ifdef SUPPORT_DEVICE_FILE
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  if (File.IsDeviceFile)
  {
    if (File.SizeDefined)
    {
      if (VirtPos >= File.Size)
        return VirtPos == File.Size ? S_OK : E_FAIL;
      UInt64 rem = File.Size - VirtPos;
      if (size > rem)
        size = (UInt32)rem;
    }
    for (;;)
    {
      const UInt32 mask = kClusterSize - 1;
      const UInt64 mask2 = ~(UInt64)mask;
      UInt64 alignedPos = VirtPos & mask2;
      if (BufSize > 0 && BufStartPos == alignedPos)
      {
        UInt32 pos = (UInt32)VirtPos & mask;
        if (pos >= BufSize)
          return S_OK;
        UInt32 rem = MyMin(BufSize - pos, size);
        memcpy(data, Buf + pos, rem);
        VirtPos += rem;
        if (processedSize)
          *processedSize += rem;
        return S_OK;
      }
      
      bool useBuf = false;
      if ((VirtPos & mask) != 0 || ((ptrdiff_t)data & mask) != 0 )
        useBuf = true;
      else
      {
        UInt64 end = VirtPos + size;
        if ((end & mask) != 0)
        {
          end &= mask2;
          if (end <= VirtPos)
            useBuf = true;
          else
            size = (UInt32)(end - VirtPos);
        }
      }
      if (!useBuf)
        break;
      if (alignedPos != PhyPos)
      {
        UInt64 realNewPosition;
        bool result = File.Seek((Int64)alignedPos, FILE_BEGIN, realNewPosition);
        if (!result)
          return ConvertBoolToHRESULT(result);
        PhyPos = realNewPosition;
      }

      BufStartPos = alignedPos;
      UInt32 readSize = kClusterSize;
      if (File.SizeDefined)
        readSize = (UInt32)MyMin(File.Size - PhyPos, (UInt64)kClusterSize);

      if (!Buf)
      {
        Buf = (Byte *)MidAlloc(kClusterSize);
        if (!Buf)
          return E_OUTOFMEMORY;
      }
      bool result = File.Read1(Buf, readSize, BufSize);
      if (!result)
        return ConvertBoolToHRESULT(result);

      if (BufSize == 0)
        return S_OK;
      PhyPos += BufSize;
    }

    if (VirtPos != PhyPos)
    {
      UInt64 realNewPosition;
      bool result = File.Seek((Int64)VirtPos, FILE_BEGIN, realNewPosition);
      if (!result)
        return ConvertBoolToHRESULT(result);
      PhyPos = VirtPos = realNewPosition;
    }
  }
  #endif

  UInt32 realProcessedSize;
  const bool result = File.ReadPart(data, size, realProcessedSize);
  if (processedSize)
    *processedSize = realProcessedSize;

  #ifdef SUPPORT_DEVICE_FILE
  VirtPos += realProcessedSize;
  PhyPos += realProcessedSize;
  #endif

  if (result)
    return S_OK;

  #else // USE_WIN_FILE
  
  if (processedSize)
    *processedSize = 0;
  const ssize_t res = File.read_part(data, (size_t)size);
  if (res != -1)
  {
    if (processedSize)
      *processedSize = (UInt32)res;
    return S_OK;
  }
  #endif // USE_WIN_FILE

  {
    const DWORD error = ::GetLastError();
    if (Callback)
      return Callback->InFileStream_On_Error(CallbackRef, error);
    if (error == 0)
      return E_FAIL;
    return HRESULT_FROM_WIN32(error);
  }
}

#ifdef UNDER_CE
STDMETHODIMP CStdInFileStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  size_t s2 = fread(data, 1, size, stdin);
  int error = ferror(stdin);
  if (processedSize)
    *processedSize = s2;
  if (s2 <= size && error == 0)
    return S_OK;
  return E_FAIL;
}
#else
STDMETHODIMP CStdInFileStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  #ifdef _WIN32
  
  DWORD realProcessedSize;
  UInt32 sizeTemp = (1 << 20);
  if (sizeTemp > size)
    sizeTemp = size;
  BOOL res = ::ReadFile(GetStdHandle(STD_INPUT_HANDLE), data, sizeTemp, &realProcessedSize, NULL);
  if (processedSize)
    *processedSize = realProcessedSize;
  if (res == FALSE && GetLastError() == ERROR_BROKEN_PIPE)
    return S_OK;
  return ConvertBoolToHRESULT(res != FALSE);
  
  #else

  if (processedSize)
    *processedSize = 0;
  ssize_t res;
  do
  {
    res = read(0, data, (size_t)size);
  }
  while (res < 0 && (errno == EINTR));
  if (res == -1)
    return GetLastError_HRESULT();
  if (processedSize)
    *processedSize = (UInt32)res;
  return S_OK;
  
  #endif
}
  
#endif

STDMETHODIMP CInFileStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  if (seekOrigin >= 3)
    return STG_E_INVALIDFUNCTION;

  #ifdef USE_WIN_FILE

  #ifdef SUPPORT_DEVICE_FILE
  if (File.IsDeviceFile && (File.SizeDefined || seekOrigin != STREAM_SEEK_END))
  {
    switch (seekOrigin)
    {
      case STREAM_SEEK_SET: break;
      case STREAM_SEEK_CUR: offset += VirtPos; break;
      case STREAM_SEEK_END: offset += File.Size; break;
      default: return STG_E_INVALIDFUNCTION;
    }
    if (offset < 0)
      return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
    VirtPos = (UInt64)offset;
    if (newPosition)
      *newPosition = (UInt64)offset;
    return S_OK;
  }
  #endif
  
  UInt64 realNewPosition = 0;
  const bool result = File.Seek(offset, seekOrigin, realNewPosition);
  const HRESULT hres = ConvertBoolToHRESULT(result);

  /* 21.07: new File.Seek() in 21.07 already returns correct (realNewPosition)
     in case of error. So we don't need additional code below */
  // if (!result) { realNewPosition = 0; File.GetPosition(realNewPosition); }
  
  #ifdef SUPPORT_DEVICE_FILE
  PhyPos = VirtPos = realNewPosition;
  #endif

  if (newPosition)
    *newPosition = realNewPosition;

  return hres;
  
  #else
  
  const off_t res = File.seek((off_t)offset, (int)seekOrigin);
  if (res == -1)
  {
    const HRESULT hres = GetLastError_HRESULT();
    if (newPosition)
      *newPosition = (UInt64)File.seekToCur();
    return hres;
  }
  if (newPosition)
    *newPosition = (UInt64)res;
  return S_OK;
  
  #endif
}

STDMETHODIMP CInFileStream::GetSize(UInt64 *size)
{
  return ConvertBoolToHRESULT(File.GetLength(*size));
}

#ifdef USE_WIN_FILE

STDMETHODIMP CInFileStream::GetProps(UInt64 *size, FILETIME *cTime, FILETIME *aTime, FILETIME *mTime, UInt32 *attrib)
{
  BY_HANDLE_FILE_INFORMATION info;
  if (File.GetFileInformation(&info))
  {
    if (size) *size = (((UInt64)info.nFileSizeHigh) << 32) + info.nFileSizeLow;
    if (cTime) *cTime = info.ftCreationTime;
    if (aTime) *aTime = info.ftLastAccessTime;
    if (mTime) *mTime = info.ftLastWriteTime;
    if (attrib) *attrib = info.dwFileAttributes;
    return S_OK;
  }
  return GetLastError_HRESULT();
}

STDMETHODIMP CInFileStream::GetProps2(CStreamFileProps *props)
{
  BY_HANDLE_FILE_INFORMATION info;
  if (File.GetFileInformation(&info))
  {
    props->Size = (((UInt64)info.nFileSizeHigh) << 32) + info.nFileSizeLow;
    props->VolID = info.dwVolumeSerialNumber;
    props->FileID_Low = (((UInt64)info.nFileIndexHigh) << 32) + info.nFileIndexLow;
    props->FileID_High = 0;
    props->NumLinks = SupportHardLinks ? info.nNumberOfLinks : 1;
    props->Attrib = info.dwFileAttributes;
    props->CTime = info.ftCreationTime;
    props->ATime = info.ftLastAccessTime;
    props->MTime = info.ftLastWriteTime;
    return S_OK;
  }
  return GetLastError_HRESULT();
}

#elif !defined(_WIN32)

STDMETHODIMP CInFileStream::GetProps(UInt64 *size, FILETIME *cTime, FILETIME *aTime, FILETIME *mTime, UInt32 *attrib)
{
  struct stat st;
  if (File.my_fstat(&st) != 0)
    return GetLastError_HRESULT();

  if (size) *size = (UInt64)st.st_size;
  #ifdef __APPLE__
  if (cTime) NWindows::NFile::NFind::timespec_To_FILETIME(st.st_ctimespec, *cTime);
  if (aTime) NWindows::NFile::NFind::timespec_To_FILETIME(st.st_atimespec, *aTime);
  if (mTime) NWindows::NFile::NFind::timespec_To_FILETIME(st.st_mtimespec, *mTime);
  #else
  if (cTime) NWindows::NFile::NFind::timespec_To_FILETIME(st.st_ctim, *cTime);
  if (aTime) NWindows::NFile::NFind::timespec_To_FILETIME(st.st_atim, *aTime);
  if (mTime) NWindows::NFile::NFind::timespec_To_FILETIME(st.st_mtim, *mTime);
  #endif
  if (attrib) *attrib = NWindows::NFile::NFind::Get_WinAttribPosix_From_PosixMode(st.st_mode);

  return S_OK;
}

// #include <stdio.h>

STDMETHODIMP CInFileStream::GetProps2(CStreamFileProps *props)
{
  struct stat st;
  if (File.my_fstat(&st) != 0)
    return GetLastError_HRESULT();

  props->Size = (UInt64)st.st_size;
  /*
    dev_t stat::st_dev:
       GCC:Linux  long unsigned int :  __dev_t
       Mac:       int
  */
  props->VolID = (UInt64)(Int64)st.st_dev;
  props->FileID_Low = st.st_ino;
  props->FileID_High = 0;
  props->NumLinks = (UInt32)st.st_nlink; // we reduce to UInt32 from (nlink_t) that is (unsigned long)
  props->Attrib = NWindows::NFile::NFind::Get_WinAttribPosix_From_PosixMode(st.st_mode);

  #ifdef __APPLE__
  NWindows::NFile::NFind::timespec_To_FILETIME(st.st_ctimespec, props->CTime);
  NWindows::NFile::NFind::timespec_To_FILETIME(st.st_atimespec, props->ATime);
  NWindows::NFile::NFind::timespec_To_FILETIME(st.st_mtimespec, props->MTime);
  #else
  NWindows::NFile::NFind::timespec_To_FILETIME(st.st_ctim, props->CTime);
  NWindows::NFile::NFind::timespec_To_FILETIME(st.st_atim, props->ATime);
  NWindows::NFile::NFind::timespec_To_FILETIME(st.st_mtim, props->MTime);
  #endif

  /*
  printf("\nGetProps2() NumLinks=%d = st_dev=%d st_ino = %d\n"
      , (unsigned)(props->NumLinks)
      , (unsigned)(st.st_dev)
      , (unsigned)(st.st_ino)
      );
  */

  return S_OK;
}

#endif

//////////////////////////
// COutFileStream

HRESULT COutFileStream::Close()
{
  return ConvertBoolToHRESULT(File.Close());
}

STDMETHODIMP COutFileStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  #ifdef USE_WIN_FILE

  UInt32 realProcessedSize;
  const bool result = File.Write(data, size, realProcessedSize);
  ProcessedSize += realProcessedSize;
  if (processedSize)
    *processedSize = realProcessedSize;
  return ConvertBoolToHRESULT(result);
  
  #else
  
  if (processedSize)
    *processedSize = 0;
  size_t realProcessedSize;
  const ssize_t res = File.write_full(data, (size_t)size, realProcessedSize);
  ProcessedSize += realProcessedSize;
  if (processedSize)
    *processedSize = (UInt32)realProcessedSize;
  if (res == -1)
    return GetLastError_HRESULT();
  return S_OK;
  
  #endif
}
  
STDMETHODIMP COutFileStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  if (seekOrigin >= 3)
    return STG_E_INVALIDFUNCTION;
  
  #ifdef USE_WIN_FILE

  UInt64 realNewPosition = 0;
  const bool result = File.Seek(offset, seekOrigin, realNewPosition);
  if (newPosition)
    *newPosition = realNewPosition;
  return ConvertBoolToHRESULT(result);
  
  #else
  
  const off_t res = File.seek((off_t)offset, (int)seekOrigin);
  if (res == -1)
    return GetLastError_HRESULT();
  if (newPosition)
    *newPosition = (UInt64)res;
  return S_OK;
  
  #endif
}

STDMETHODIMP COutFileStream::SetSize(UInt64 newSize)
{
  return ConvertBoolToHRESULT(File.SetLength_KeepPosition(newSize));
}

HRESULT COutFileStream::GetSize(UInt64 *size)
{
  return ConvertBoolToHRESULT(File.GetLength(*size));
}

#ifdef UNDER_CE

STDMETHODIMP CStdOutFileStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  size_t s2 = fwrite(data, 1, size, stdout);
  if (processedSize)
    *processedSize = s2;
  return (s2 == size) ? S_OK : E_FAIL;
}

#else

STDMETHODIMP CStdOutFileStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;

  #ifdef _WIN32

  UInt32 realProcessedSize;
  BOOL res = TRUE;
  if (size > 0)
  {
    // Seems that Windows doesn't like big amounts writing to stdout.
    // So we limit portions by 32KB.
    UInt32 sizeTemp = (1 << 15);
    if (sizeTemp > size)
      sizeTemp = size;
    res = ::WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
        data, sizeTemp, (DWORD *)&realProcessedSize, NULL);
    _size += realProcessedSize;
    size -= realProcessedSize;
    data = (const void *)((const Byte *)data + realProcessedSize);
    if (processedSize)
      *processedSize += realProcessedSize;
  }
  return ConvertBoolToHRESULT(res != FALSE);

  #else
  
  ssize_t res;

  do
  {
    res = write(1, data, (size_t)size);
  }
  while (res < 0 && (errno == EINTR));
  
  if (res == -1)
    return GetLastError_HRESULT();

  _size += (size_t)res;
  if (processedSize)
    *processedSize = (UInt32)res;
  return S_OK;
  
  #endif
}

#endif
