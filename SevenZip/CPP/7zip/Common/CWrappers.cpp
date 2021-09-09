// CWrappers.c

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "CWrappers.h"

#include "StreamUtils.h"

SRes HRESULT_To_SRes(HRESULT res, SRes defaultRes) throw()
{
  switch (res)
  {
    case S_OK: return SZ_OK;
    case E_OUTOFMEMORY: return SZ_ERROR_MEM;
    case E_INVALIDARG: return SZ_ERROR_PARAM;
    case E_ABORT: return SZ_ERROR_PROGRESS;
    case S_FALSE: return SZ_ERROR_DATA;
    case E_NOTIMPL: return SZ_ERROR_UNSUPPORTED;
  }
  return defaultRes;
}


HRESULT SResToHRESULT(SRes res) throw()
{
  switch (res)
  {
    case SZ_OK: return S_OK;
    
    case SZ_ERROR_DATA:
    case SZ_ERROR_CRC:
    case SZ_ERROR_INPUT_EOF:
      return S_FALSE;
    
    case SZ_ERROR_MEM: return E_OUTOFMEMORY;
    case SZ_ERROR_PARAM: return E_INVALIDARG;
    case SZ_ERROR_PROGRESS: return E_ABORT;
    case SZ_ERROR_UNSUPPORTED: return E_NOTIMPL;
    // case SZ_ERROR_OUTPUT_EOF:
    // case SZ_ERROR_READ:
    // case SZ_ERROR_WRITE:
    // case SZ_ERROR_THREAD:
    // case SZ_ERROR_ARCHIVE:
    // case SZ_ERROR_NO_ARCHIVE:
    // return E_FAIL;
  }
  if (res < 0)
    return res;
  return E_FAIL;
}


#define PROGRESS_UNKNOWN_VALUE ((UInt64)(Int64)-1)

#define CONVERT_PR_VAL(x) (x == PROGRESS_UNKNOWN_VALUE ? NULL : &x)


static SRes CompressProgress(const ICompressProgress *pp, UInt64 inSize, UInt64 outSize) throw()
{
  CCompressProgressWrap *p = CONTAINER_FROM_VTBL(pp, CCompressProgressWrap, vt);
  p->Res = p->Progress->SetRatioInfo(CONVERT_PR_VAL(inSize), CONVERT_PR_VAL(outSize));
  return HRESULT_To_SRes(p->Res, SZ_ERROR_PROGRESS);
}

void CCompressProgressWrap::Init(ICompressProgressInfo *progress) throw()
{
  vt.Progress = CompressProgress;
  Progress = progress;
  Res = SZ_OK;
}

static const UInt32 kStreamStepSize = (UInt32)1 << 31;

static SRes MyRead(const ISeqInStream *pp, void *data, size_t *size) throw()
{
  CSeqInStreamWrap *p = CONTAINER_FROM_VTBL(pp, CSeqInStreamWrap, vt);
  UInt32 curSize = ((*size < kStreamStepSize) ? (UInt32)*size : kStreamStepSize);
  p->Res = (p->Stream->Read(data, curSize, &curSize));
  *size = curSize;
  p->Processed += curSize;
  if (p->Res == S_OK)
    return SZ_OK;
  return HRESULT_To_SRes(p->Res, SZ_ERROR_READ);
}

static size_t MyWrite(const ISeqOutStream *pp, const void *data, size_t size) throw()
{
  CSeqOutStreamWrap *p = CONTAINER_FROM_VTBL(pp, CSeqOutStreamWrap, vt);
  if (p->Stream)
  {
    p->Res = WriteStream(p->Stream, data, size);
    if (p->Res != 0)
      return 0;
  }
  else
    p->Res = S_OK;
  p->Processed += size;
  return size;
}


void CSeqInStreamWrap::Init(ISequentialInStream *stream) throw()
{
  vt.Read = MyRead;
  Stream = stream;
  Processed = 0;
  Res = S_OK;
}

void CSeqOutStreamWrap::Init(ISequentialOutStream *stream) throw()
{
  vt.Write = MyWrite;
  Stream = stream;
  Res = SZ_OK;
  Processed = 0;
}


static SRes InStreamWrap_Read(const ISeekInStream *pp, void *data, size_t *size) throw()
{
  CSeekInStreamWrap *p = CONTAINER_FROM_VTBL(pp, CSeekInStreamWrap, vt);
  UInt32 curSize = ((*size < kStreamStepSize) ? (UInt32)*size : kStreamStepSize);
  p->Res = p->Stream->Read(data, curSize, &curSize);
  *size = curSize;
  return (p->Res == S_OK) ? SZ_OK : SZ_ERROR_READ;
}

static SRes InStreamWrap_Seek(const ISeekInStream *pp, Int64 *offset, ESzSeek origin) throw()
{
  CSeekInStreamWrap *p = CONTAINER_FROM_VTBL(pp, CSeekInStreamWrap, vt);
  UInt32 moveMethod;
  switch (origin)
  {
    case SZ_SEEK_SET: moveMethod = STREAM_SEEK_SET; break;
    case SZ_SEEK_CUR: moveMethod = STREAM_SEEK_CUR; break;
    case SZ_SEEK_END: moveMethod = STREAM_SEEK_END; break;
    default: return SZ_ERROR_PARAM;
  }
  UInt64 newPosition;
  p->Res = p->Stream->Seek(*offset, moveMethod, &newPosition);
  *offset = (Int64)newPosition;
  return (p->Res == S_OK) ? SZ_OK : SZ_ERROR_READ;
}

void CSeekInStreamWrap::Init(IInStream *stream) throw()
{
  Stream = stream;
  vt.Read = InStreamWrap_Read;
  vt.Seek = InStreamWrap_Seek;
  Res = S_OK;
}


/* ---------- CByteInBufWrap ---------- */

void CByteInBufWrap::Free() throw()
{
  ::MidFree(Buf);
  Buf = NULL;
}

bool CByteInBufWrap::Alloc(UInt32 size) throw()
{
  if (!Buf || size != Size)
  {
    Free();
    Lim = Cur = Buf = (Byte *)::MidAlloc((size_t)size);
    Size = size;
  }
  return (Buf != NULL);
}

Byte CByteInBufWrap::ReadByteFromNewBlock() throw()
{
  if (!Extra && Res == S_OK)
  {
    UInt32 avail;
    Res = Stream->Read(Buf, Size, &avail);
    Processed += (size_t)(Cur - Buf);
    Cur = Buf;
    Lim = Buf + avail;
    if (avail != 0)
      return *Cur++;
  }
  Extra = true;
  return 0;
}

static Byte Wrap_ReadByte(const IByteIn *pp) throw()
{
  CByteInBufWrap *p = CONTAINER_FROM_VTBL_CLS(pp, CByteInBufWrap, vt);
  if (p->Cur != p->Lim)
    return *p->Cur++;
  return p->ReadByteFromNewBlock();
}

CByteInBufWrap::CByteInBufWrap(): Buf(NULL)
{
  vt.Read = Wrap_ReadByte;
}



/* ---------- CByteOutBufWrap ---------- */

/*
void CLookToSequentialWrap::Free() throw()
{
  ::MidFree(BufBase);
  BufBase = NULL;
}

bool CLookToSequentialWrap::Alloc(UInt32 size) throw()
{
  if (!BufBase || size != Size)
  {
    Free();
    BufBase = (Byte *)::MidAlloc((size_t)size);
    Size = size;
  }
  return (BufBase != NULL);
}
*/

/*
EXTERN_C_BEGIN

void CLookToSequentialWrap_Look(ILookInSeqStream *pp)
{
  CLookToSequentialWrap *p = (CLookToSequentialWrap *)pp->Obj;

  if (p->Extra || p->Res != S_OK)
    return;
  {
    UInt32 avail;
    p->Res = p->Stream->Read(p->BufBase, p->Size, &avail);
    p->Processed += avail;
    pp->Buf = p->BufBase;
    pp->Limit = pp->Buf + avail;
    if (avail == 0)
      p->Extra = true;
  }
}

EXTERN_C_END
*/


/* ---------- CByteOutBufWrap ---------- */

void CByteOutBufWrap::Free() throw()
{
  ::MidFree(Buf);
  Buf = NULL;
}

bool CByteOutBufWrap::Alloc(size_t size) throw()
{
  if (!Buf || size != Size)
  {
    Free();
    Buf = (Byte *)::MidAlloc(size);
    Size = size;
  }
  return (Buf != NULL);
}

HRESULT CByteOutBufWrap::Flush() throw()
{
  if (Res == S_OK)
  {
    const size_t size = (size_t)(Cur - Buf);
    Res = WriteStream(Stream, Buf, size);
    if (Res == S_OK)
      Processed += size;
    // else throw 11;
  }
  Cur = Buf; // reset pointer for later Wrap_WriteByte()
  return Res;
}

static void Wrap_WriteByte(const IByteOut *pp, Byte b) throw()
{
  CByteOutBufWrap *p = CONTAINER_FROM_VTBL_CLS(pp, CByteOutBufWrap, vt);
  Byte *dest = p->Cur;
  *dest = b;
  p->Cur = ++dest;
  if (dest == p->Lim)
    p->Flush();
}

CByteOutBufWrap::CByteOutBufWrap() throw(): Buf(NULL), Size(0)
{
  vt.Write = Wrap_WriteByte;
}


/* ---------- CLookOutWrap ---------- */

/*
void CLookOutWrap::Free() throw()
{
  ::MidFree(Buf);
  Buf = NULL;
}

bool CLookOutWrap::Alloc(size_t size) throw()
{
  if (!Buf || size != Size)
  {
    Free();
    Buf = (Byte *)::MidAlloc(size);
    Size = size;
  }
  return (Buf != NULL);
}

static size_t LookOutWrap_GetOutBuf(const ILookOutStream *pp, void **buf) throw()
{
  CLookOutWrap *p = CONTAINER_FROM_VTBL_CLS(pp, CLookOutWrap, vt);
  *buf = p->Buf;
  return p->Size;
}

static size_t LookOutWrap_Write(const ILookOutStream *pp, size_t size) throw()
{
  CLookOutWrap *p = CONTAINER_FROM_VTBL_CLS(pp, CLookOutWrap, vt);
  if (p->Res == S_OK && size != 0)
  {
    p->Res = WriteStream(p->Stream, p->Buf, size);
    if (p->Res == S_OK)
    {
      p->Processed += size;
      return size;
    }
  }
  return 0;
}

CLookOutWrap::CLookOutWrap() throw(): Buf(NULL), Size(0)
{
  vt.GetOutBuf = LookOutWrap_GetOutBuf;
  vt.Write = LookOutWrap_Write;
}
*/
