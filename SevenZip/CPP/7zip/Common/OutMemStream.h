// OutMemStream.h

#ifndef __OUT_MEM_STREAM_H
#define __OUT_MEM_STREAM_H

#include "../../Common/MyCom.h"

#include "MemBlocks.h"

class COutMemStream:
  public IOutStream,
  public CMyUnknownImp
{
  CMemBlockManagerMt *_memManager;
  unsigned _curBlockIndex;
  size_t _curBlockPos;
  bool _realStreamMode;

  bool _unlockEventWasSent;
  NWindows::NSynchronization::CAutoResetEvent_WFMO StopWritingEvent;
  NWindows::NSynchronization::CAutoResetEvent_WFMO WriteToRealStreamEvent;
  // NWindows::NSynchronization::CAutoResetEvent NoLockEvent;

  HRESULT StopWriteResult;
  CMemLockBlocks Blocks;

  UInt64 GetPos() const { return (UInt64)_curBlockIndex * _memManager->GetBlockSize() + _curBlockPos; }

  CMyComPtr<ISequentialOutStream> OutSeqStream;
  CMyComPtr<IOutStream> OutStream;

public:

  
  HRes CreateEvents(SYNC_PARAM_DECL(synchro))
  {
    WRes wres = StopWritingEvent.CreateIfNotCreated_Reset(SYNC_WFMO(synchro));
    if (wres == 0)
      wres = WriteToRealStreamEvent.CreateIfNotCreated_Reset(SYNC_WFMO(synchro));
    return HRESULT_FROM_WIN32(wres);
  }

  void SetOutStream(IOutStream *outStream)
  {
    OutStream = outStream;
    OutSeqStream = outStream;
  }

  void SetSeqOutStream(ISequentialOutStream *outStream)
  {
    OutStream = NULL;
    OutSeqStream = outStream;
  }

  void ReleaseOutStream()
  {
    OutStream.Release();
    OutSeqStream.Release();
  }

  COutMemStream(CMemBlockManagerMt *memManager):
      _memManager(memManager)
  {
    /*
    #ifndef _WIN32
    StopWritingEvent._sync       =
    WriteToRealStreamEvent._sync =  &memManager->Synchro;
    #endif
    */
  }

  ~COutMemStream() { Free(); }
  void Free();

  void Init();
  HRESULT WriteToRealStream();

  void DetachData(CMemLockBlocks &blocks);

  bool WasUnlockEventSent() const { return _unlockEventWasSent; }

  void SetRealStreamMode()
  {
    _unlockEventWasSent = true;
    WriteToRealStreamEvent.Set();
  }

  /*
  void SetNoLockMode()
  {
    _unlockEventWasSent = true;
    NoLockEvent.Set();
  }
  */

  void StopWriting(HRESULT res)
  {
    StopWriteResult = res;
    StopWritingEvent.Set();
  }

  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
  STDMETHOD(SetSize)(UInt64 newSize);
};

#endif
