// 7zOut.h

#ifndef ZIP7_INC_7Z_OUT_H
#define ZIP7_INC_7Z_OUT_H

#include "7zCompressionMode.h"
#include "7zEncode.h"
#include "7zHeader.h"
#include "7zItem.h"

#include "../../Common/OutBuffer.h"
#include "../../Common/StreamUtils.h"

namespace NArchive {
namespace N7z {

const unsigned k_StartHeadersRewriteSize = 32;

class CWriteBufferLoc
{
  Byte *_data;
  size_t _size;
  size_t _pos;
public:
  CWriteBufferLoc(): _size(0), _pos(0) {}
  void Init(Byte *data, size_t size)
  {
    _data = data;
    _size = size;
    _pos = 0;
  }
  void WriteBytes(const void *data, size_t size)
  {
    if (size == 0)
      return;
    if (size > _size - _pos)
      throw 1;
    memcpy(_data + _pos, data, size);
    _pos += size;
  }
  void WriteByte(Byte b)
  {
    if (_size == _pos)
      throw 1;
    _data[_pos++] = b;
  }
  size_t GetPos() const { return _pos; }
};


struct CHeaderOptions
{
  bool CompressMainHeader;
  /*
  bool WriteCTime;
  bool WriteATime;
  bool WriteMTime;
  */

  CHeaderOptions():
      CompressMainHeader(true)
      /*
      , WriteCTime(false)
      , WriteATime(false)
      , WriteMTime(true)
      */
      {}
};


struct CFileItem2
{
  UInt64 CTime;
  UInt64 ATime;
  UInt64 MTime;
  UInt64 StartPos;
  UInt32 Attrib;

  bool CTimeDefined;
  bool ATimeDefined;
  bool MTimeDefined;
  bool StartPosDefined;
  bool AttribDefined;
  bool IsAnti;
  // bool IsAux;

  /*
  void Init()
  {
    CTimeDefined = false;
    ATimeDefined = false;
    MTimeDefined = false;
    StartPosDefined = false;
    AttribDefined = false;
    IsAnti = false;
    // IsAux = false;
  }
  */
};


struct COutFolders
{
  CUInt32DefVector FolderUnpackCRCs; // Now we use it for headers only.

  CRecordVector<CNum> NumUnpackStreamsVector;
  CRecordVector<UInt64> CoderUnpackSizes; // including unpack sizes of bond coders

  void OutFoldersClear()
  {
    FolderUnpackCRCs.Clear();
    NumUnpackStreamsVector.Clear();
    CoderUnpackSizes.Clear();
  }

  void OutFoldersReserveDown()
  {
    FolderUnpackCRCs.ReserveDown();
    NumUnpackStreamsVector.ReserveDown();
    CoderUnpackSizes.ReserveDown();
  }
};


struct CArchiveDatabaseOut: public COutFolders
{
  CRecordVector<UInt64> PackSizes;
  CUInt32DefVector PackCRCs;
  CObjectVector<CFolder> Folders;

  CRecordVector<CFileItem> Files;
  UStringVector Names;
  CUInt64DefVector CTime;
  CUInt64DefVector ATime;
  CUInt64DefVector MTime;
  CUInt64DefVector StartPos;
  CUInt32DefVector Attrib;
  CBoolVector IsAnti;

  /*
  CBoolVector IsAux;

  CByteBuffer SecureBuf;
  CRecordVector<UInt32> SecureSizes;
  CRecordVector<UInt32> SecureIDs;

  void ClearSecure()
  {
    SecureBuf.Free();
    SecureSizes.Clear();
    SecureIDs.Clear();
  }
  */

  void Clear()
  {
    OutFoldersClear();

    PackSizes.Clear();
    PackCRCs.Clear();
    Folders.Clear();
  
    Files.Clear();
    Names.Clear();
    CTime.Clear();
    ATime.Clear();
    MTime.Clear();
    StartPos.Clear();
    Attrib.Clear();
    IsAnti.Clear();

    /*
    IsAux.Clear();
    ClearSecure();
    */
  }

  void ReserveDown()
  {
    OutFoldersReserveDown();

    PackSizes.ReserveDown();
    PackCRCs.ReserveDown();
    Folders.ReserveDown();
    
    Files.ReserveDown();
    Names.ReserveDown();
    CTime.ReserveDown();
    ATime.ReserveDown();
    MTime.ReserveDown();
    StartPos.ReserveDown();
    Attrib.ReserveDown();
    IsAnti.ReserveDown();

    /*
    IsAux.ReserveDown();
    */
  }

  bool IsEmpty() const
  {
    return (
      PackSizes.IsEmpty() &&
      NumUnpackStreamsVector.IsEmpty() &&
      Folders.IsEmpty() &&
      Files.IsEmpty());
  }

  bool CheckNumFiles() const
  {
    unsigned size = Files.Size();
    return (
           CTime.CheckSize(size)
        && ATime.CheckSize(size)
        && MTime.CheckSize(size)
        && StartPos.CheckSize(size)
        && Attrib.CheckSize(size)
        && (size == IsAnti.Size() || IsAnti.Size() == 0));
  }

  bool IsItemAnti(unsigned index) const { return (index < IsAnti.Size() && IsAnti[index]); }
  // bool IsItemAux(unsigned index) const { return (index < IsAux.Size() && IsAux[index]); }

  void SetItem_Anti(unsigned index, bool isAnti)
  {
    while (index >= IsAnti.Size())
      IsAnti.Add(false);
    IsAnti[index] = isAnti;
  }
  /*
  void SetItem_Aux(unsigned index, bool isAux)
  {
    while (index >= IsAux.Size())
      IsAux.Add(false);
    IsAux[index] = isAux;
  }
  */

  void AddFile(const CFileItem &file, const CFileItem2 &file2, const UString &name);
};


class COutArchive
{
  HRESULT WriteDirect(const void *data, UInt32 size) { return WriteStream(SeqStream, data, size); }
  
  UInt64 GetPos() const;
  void WriteBytes(const void *data, size_t size);
  void WriteBytes(const CByteBuffer &data) { WriteBytes(data, data.Size()); }
  void WriteByte(Byte b);
  void WriteUInt32(UInt32 value);
  void WriteUInt64(UInt64 value);
  void WriteNumber(UInt64 value);
  void WriteID(UInt64 value) { WriteNumber(value); }

  void WriteFolder(const CFolder &folder);
  HRESULT WriteFileHeader(const CFileItem &itemInfo);
  void WriteBoolVector(const CBoolVector &boolVector);
  void WritePropBoolVector(Byte id, const CBoolVector &boolVector);

  void WriteHashDigests(const CUInt32DefVector &digests);

  void WritePackInfo(
      UInt64 dataOffset,
      const CRecordVector<UInt64> &packSizes,
      const CUInt32DefVector &packCRCs);

  void WriteUnpackInfo(
      const CObjectVector<CFolder> &folders,
      const COutFolders &outFolders);

  void WriteSubStreamsInfo(
      const CObjectVector<CFolder> &folders,
      const COutFolders &outFolders,
      const CRecordVector<UInt64> &unpackSizes,
      const CUInt32DefVector &digests);

  void SkipToAligned(unsigned pos, unsigned alignShifts);
  void WriteAlignedBools(const CBoolVector &v, unsigned numDefined, Byte type, unsigned itemSizeShifts);
  void WriteUInt64DefVector(const CUInt64DefVector &v, Byte type);

  HRESULT EncodeStream(
      DECL_EXTERNAL_CODECS_LOC_VARS
      CEncoder &encoder, const CByteBuffer &data,
      CRecordVector<UInt64> &packSizes, CObjectVector<CFolder> &folders, COutFolders &outFolders);
  void WriteHeader(
      const CArchiveDatabaseOut &db,
      // const CHeaderOptions &headerOptions,
      UInt64 &headerOffset);
  
  bool _countMode;
  bool _writeToStream;
  bool _useAlign;
  #ifdef Z7_7Z_VOL
  bool _endMarker;
  #endif
  UInt32 _crc;
  size_t _countSize;
  CWriteBufferLoc _outByte2;
  COutBuffer _outByte;
  UInt64 _signatureHeaderPos;
  CMyComPtr<IOutStream> Stream;

  #ifdef Z7_7Z_VOL
  HRESULT WriteFinishSignature();
  HRESULT WriteFinishHeader(const CFinishHeader &h);
  #endif
  HRESULT WriteStartHeader(const CStartHeader &h);

public:
  CMyComPtr<ISequentialOutStream> SeqStream;

  COutArchive() { _outByte.Create(1 << 16); }
  HRESULT Create_and_WriteStartPrefix(ISequentialOutStream *stream /* , bool endMarker */);
  void Close();
  HRESULT WriteDatabase(
      DECL_EXTERNAL_CODECS_LOC_VARS
      const CArchiveDatabaseOut &db,
      const CCompressionMethodMode *options,
      const CHeaderOptions &headerOptions);

  #ifdef Z7_7Z_VOL
  static UInt32 GetVolHeadersSize(UInt64 dataSize, int nameLength = 0, bool props = false);
  static UInt64 GetVolPureSize(UInt64 volSize, int nameLength = 0, bool props = false);
  #endif
};

}}

#endif
