// CoderMixer2.h

#ifndef __CODER_MIXER2_H
#define __CODER_MIXER2_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyVector.h"

#include "../../ICoder.h"

#include "../../Common/CreateCoder.h"

#ifdef _7ZIP_ST
  #define USE_MIXER_ST
#else
  #define USE_MIXER_MT
  #ifndef _SFX
    #define USE_MIXER_ST
  #endif
#endif

#ifdef USE_MIXER_MT
#include "../../Common/StreamBinder.h"
#include "../../Common/VirtThread.h"
#endif



#ifdef USE_MIXER_ST

class CSequentialInStreamCalcSize:
  public ISequentialInStream,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ISequentialInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
private:
  CMyComPtr<ISequentialInStream> _stream;
  UInt64 _size;
  bool _wasFinished;
public:
  void SetStream(ISequentialInStream *stream) { _stream = stream;  }
  void Init()
  {
    _size = 0;
    _wasFinished = false;
  }
  void ReleaseStream() { _stream.Release(); }
  UInt64 GetSize() const { return _size; }
  bool WasFinished() const { return _wasFinished; }
};


class COutStreamCalcSize:
  public ISequentialOutStream,
  public IOutStreamFinish,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
public:
  MY_UNKNOWN_IMP2(ISequentialOutStream, IOutStreamFinish)
  
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(OutStreamFinish)();
  
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init() { _size = 0; }
  UInt64 GetSize() const { return _size; }
};

#endif


  
namespace NCoderMixer2 {

struct CBond
{
  UInt32 PackIndex;
  UInt32 UnpackIndex;

  UInt32 Get_InIndex(bool encodeMode) const { return encodeMode ? UnpackIndex : PackIndex; }
  UInt32 Get_OutIndex(bool encodeMode) const { return encodeMode ? PackIndex : UnpackIndex; }
};


struct CCoderStreamsInfo
{
  UInt32 NumStreams;
};


struct CBindInfo
{
  CRecordVector<CCoderStreamsInfo> Coders;
  CRecordVector<CBond> Bonds;
  CRecordVector<UInt32> PackStreams;
  unsigned UnpackCoder;

  unsigned GetNum_Bonds_and_PackStreams() const { return Bonds.Size() + PackStreams.Size(); }

  int FindBond_for_PackStream(UInt32 packStream) const
  {
    FOR_VECTOR (i, Bonds)
      if (Bonds[i].PackIndex == packStream)
        return (int)i;
    return -1;
  }

  int FindBond_for_UnpackStream(UInt32 unpackStream) const
  {
    FOR_VECTOR (i, Bonds)
      if (Bonds[i].UnpackIndex == unpackStream)
        return (int)i;
    return -1;
  }

  bool SetUnpackCoder()
  {
    bool isOk = false;
    FOR_VECTOR(i, Coders)
    {
      if (FindBond_for_UnpackStream(i) < 0)
      {
        if (isOk)
          return false;
        UnpackCoder = i;
        isOk = true;
      }
    }
    return isOk;
  }
  
  bool IsStream_in_PackStreams(UInt32 streamIndex) const
  {
    return FindStream_in_PackStreams(streamIndex) >= 0;
  }

  int FindStream_in_PackStreams(UInt32 streamIndex) const
  {
    FOR_VECTOR(i, PackStreams)
      if (PackStreams[i] == streamIndex)
        return (int)i;
    return -1;
  }

  
  // that function is used before Maps is calculated

  UInt32 GetStream_for_Coder(UInt32 coderIndex) const
  {
    UInt32 streamIndex = 0;
    for (UInt32 i = 0; i < coderIndex; i++)
      streamIndex += Coders[i].NumStreams;
    return streamIndex;
  }
  
  // ---------- Maps Section ----------
  
  CRecordVector<UInt32> Coder_to_Stream;
  CRecordVector<UInt32> Stream_to_Coder;

  void ClearMaps();
  bool CalcMapsAndCheck();

  // ---------- End of Maps Section ----------

  void Clear()
  {
    Coders.Clear();
    Bonds.Clear();
    PackStreams.Clear();

    ClearMaps();
  }
  
  void GetCoder_for_Stream(UInt32 streamIndex, UInt32 &coderIndex, UInt32 &coderStreamIndex) const
  {
    coderIndex = Stream_to_Coder[streamIndex];
    coderStreamIndex = streamIndex - Coder_to_Stream[coderIndex];
  }
};



class CCoder
{
  CLASS_NO_COPY(CCoder);
public:
  CMyComPtr<ICompressCoder> Coder;
  CMyComPtr<ICompressCoder2> Coder2;
  UInt32 NumStreams;

  UInt64 UnpackSize;
  const UInt64 *UnpackSizePointer;

  CRecordVector<UInt64> PackSizes;
  CRecordVector<const UInt64 *> PackSizePointers;

  bool Finish;

  CCoder(): Finish(false) {}

  void SetCoderInfo(const UInt64 *unpackSize, const UInt64 * const *packSizes, bool finish);

  HRESULT CheckDataAfterEnd(bool &dataAfterEnd_Error /* , bool &InternalPackSizeError */) const;

  IUnknown *GetUnknown() const
  {
    return Coder ? (IUnknown *)Coder : (IUnknown *)Coder2;
  }

  HRESULT QueryInterface(REFGUID iid, void** pp) const
  {
    return GetUnknown()->QueryInterface(iid, pp);
  }
};



class CMixer
{
  bool Is_PackSize_Correct_for_Stream(UInt32 streamIndex);

protected:
  CBindInfo _bi;

  int FindBond_for_Stream(bool forInputStream, UInt32 streamIndex) const
  {
    if (EncodeMode == forInputStream)
      return _bi.FindBond_for_UnpackStream(streamIndex);
    else
      return _bi.FindBond_for_PackStream(streamIndex);
  }

  CBoolVector IsFilter_Vector;
  CBoolVector IsExternal_Vector;
  bool EncodeMode;
public:
  unsigned MainCoderIndex;

  // bool InternalPackSizeError;

  CMixer(bool encodeMode):
      EncodeMode(encodeMode),
      MainCoderIndex(0)
      // , InternalPackSizeError(false)
      {}

  virtual ~CMixer() {};
  /*
  Sequence of calling:

      SetBindInfo();
      for each coder
        AddCoder();
      SelectMainCoder();
 
      for each file
      {
        ReInit()
        for each coder
          SetCoderInfo();
        Code();
      }
  */

  virtual HRESULT SetBindInfo(const CBindInfo &bindInfo)
  {
    _bi = bindInfo;
    IsFilter_Vector.Clear();
    MainCoderIndex = 0;
    return S_OK;
  }

  virtual void AddCoder(const CCreatedCoder &cod) = 0;
  virtual CCoder &GetCoder(unsigned index) = 0;
  virtual void SelectMainCoder(bool useFirst) = 0;
  virtual HRESULT ReInit2() = 0;
  virtual void SetCoderInfo(unsigned coderIndex, const UInt64 *unpackSize, const UInt64 * const *packSizes, bool finish) = 0;
  virtual HRESULT Code(
      ISequentialInStream * const *inStreams,
      ISequentialOutStream * const *outStreams,
      ICompressProgressInfo *progress,
      bool &dataAfterEnd_Error) = 0;
  virtual UInt64 GetBondStreamSize(unsigned bondIndex) const = 0;

  bool Is_UnpackSize_Correct_for_Coder(UInt32 coderIndex);
  bool Is_PackSize_Correct_for_Coder(UInt32 coderIndex);
  bool IsThere_ExternalCoder_in_PackTree(UInt32 coderIndex);
};




#ifdef USE_MIXER_ST

struct CCoderST: public CCoder
{
  bool CanRead;
  bool CanWrite;
  
  CCoderST(): CanRead(false), CanWrite(false) {}
};


struct CStBinderStream
{
  CSequentialInStreamCalcSize *InStreamSpec;
  COutStreamCalcSize *OutStreamSpec;
  CMyComPtr<IUnknown> StreamRef;

  CStBinderStream(): InStreamSpec(NULL), OutStreamSpec(NULL) {}
};


class CMixerST:
  public IUnknown,
  public CMixer,
  public CMyUnknownImp
{
  CLASS_NO_COPY(CMixerST)

  HRESULT GetInStream2(ISequentialInStream * const *inStreams, /* const UInt64 * const *inSizes, */
      UInt32 outStreamIndex, ISequentialInStream **inStreamRes);
  HRESULT GetInStream(ISequentialInStream * const *inStreams, /* const UInt64 * const *inSizes, */
      UInt32 inStreamIndex, ISequentialInStream **inStreamRes);
  HRESULT GetOutStream(ISequentialOutStream * const *outStreams, /* const UInt64 * const *outSizes, */
      UInt32 outStreamIndex, ISequentialOutStream **outStreamRes);

  HRESULT FinishStream(UInt32 streamIndex);
  HRESULT FinishCoder(UInt32 coderIndex);

public:
  CObjectVector<CCoderST> _coders;
  
  CObjectVector<CStBinderStream> _binderStreams;

  MY_UNKNOWN_IMP

  CMixerST(bool encodeMode);
  ~CMixerST();

  virtual void AddCoder(const CCreatedCoder &cod);
  virtual CCoder &GetCoder(unsigned index);
  virtual void SelectMainCoder(bool useFirst);
  virtual HRESULT ReInit2();
  virtual void SetCoderInfo(unsigned coderIndex, const UInt64 *unpackSize, const UInt64 * const *packSizes, bool finish)
    { _coders[coderIndex].SetCoderInfo(unpackSize, packSizes, finish); }
  virtual HRESULT Code(
      ISequentialInStream * const *inStreams,
      ISequentialOutStream * const *outStreams,
      ICompressProgressInfo *progress,
      bool &dataAfterEnd_Error);
  virtual UInt64 GetBondStreamSize(unsigned bondIndex) const;

  HRESULT GetMainUnpackStream(
      ISequentialInStream * const *inStreams,
      ISequentialInStream **inStreamRes);
};

#endif




#ifdef USE_MIXER_MT

class CCoderMT: public CCoder, public CVirtThread
{
  CLASS_NO_COPY(CCoderMT)
  CRecordVector<ISequentialInStream*> InStreamPointers;
  CRecordVector<ISequentialOutStream*> OutStreamPointers;

private:
  void Execute();
public:
  bool EncodeMode;
  HRESULT Result;
  CObjectVector< CMyComPtr<ISequentialInStream> > InStreams;
  CObjectVector< CMyComPtr<ISequentialOutStream> > OutStreams;

  void Release()
  {
    InStreamPointers.Clear();
    OutStreamPointers.Clear();
    unsigned i;
    for (i = 0; i < InStreams.Size(); i++)
      InStreams[i].Release();
    for (i = 0; i < OutStreams.Size(); i++)
      OutStreams[i].Release();
  }

  class CReleaser
  {
    CLASS_NO_COPY(CReleaser)
    CCoderMT &_c;
  public:
    CReleaser(CCoderMT &c): _c(c) {}
    ~CReleaser() { _c.Release(); }
  };

  CCoderMT(): EncodeMode(false) {}
  virtual ~CCoderMT() { CVirtThread::WaitThreadFinish(); }
  
  void Code(ICompressProgressInfo *progress);
};


class CMixerMT:
  public IUnknown,
  public CMixer,
  public CMyUnknownImp
{
  CLASS_NO_COPY(CMixerMT)

  CObjectVector<CStreamBinder> _streamBinders;

  HRESULT Init(ISequentialInStream * const *inStreams, ISequentialOutStream * const *outStreams);
  HRESULT ReturnIfError(HRESULT code);

  // virtual ~CMixerMT() {};
public:
  CObjectVector<CCoderMT> _coders;

  MY_UNKNOWN_IMP

  virtual HRESULT SetBindInfo(const CBindInfo &bindInfo);
  virtual void AddCoder(const CCreatedCoder &cod);
  virtual CCoder &GetCoder(unsigned index);
  virtual void SelectMainCoder(bool useFirst);
  virtual HRESULT ReInit2();
  virtual void SetCoderInfo(unsigned coderIndex, const UInt64 *unpackSize, const UInt64 * const *packSizes, bool finish)
    { _coders[coderIndex].SetCoderInfo(unpackSize, packSizes, finish); }
  virtual HRESULT Code(
      ISequentialInStream * const *inStreams,
      ISequentialOutStream * const *outStreams,
      ICompressProgressInfo *progress,
      bool &dataAfterEnd_Error);
  virtual UInt64 GetBondStreamSize(unsigned bondIndex) const;

  CMixerMT(bool encodeMode): CMixer(encodeMode) {}
};

#endif

}

#endif
