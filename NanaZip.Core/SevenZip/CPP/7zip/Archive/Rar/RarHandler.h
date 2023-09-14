// RarHandler.h

#ifndef ZIP7_INC_RAR_HANDLER_H
#define ZIP7_INC_RAR_HANDLER_H

#include "../IArchive.h"

#include "../../Common/CreateCoder.h"

#include "RarItem.h"

namespace NArchive {
namespace NRar {

struct CInArcInfo
{
  UInt32 Flags;
  Byte EncryptVersion;
  
  UInt64 StartPos;
  UInt64 EndPos;
  UInt64 FileSize;

  UInt32 EndFlags;
  UInt32 VolNumber;
  UInt32 DataCRC;
  bool EndOfArchive_was_Read;

  CInArcInfo(): EndFlags(0), VolNumber(0), EndOfArchive_was_Read(false) {}

  UInt64 GetPhySize() const { return EndPos - StartPos; }

  bool ExtraZeroTail_is_Possible() const { return IsVolume() && IsRecovery() && EndOfArchive_was_Read; }

  bool IsVolume()           const { return (Flags & NHeader::NArchive::kVolume) != 0; }
  bool IsCommented()        const { return (Flags & NHeader::NArchive::kComment) != 0; }
  // kLock
  bool IsSolid()            const { return (Flags & NHeader::NArchive::kSolid) != 0; }
  bool HaveNewVolumeName()  const { return (Flags & NHeader::NArchive::kNewVolName) != 0; }
  // kAuthenticity
  bool IsRecovery()         const { return (Flags & NHeader::NArchive::kRecovery) != 0; }
  bool IsEncrypted()        const { return (Flags & NHeader::NArchive::kBlockEncryption) != 0; }
  bool IsFirstVolume()      const { return (Flags & NHeader::NArchive::kFirstVolume) != 0; }

  // bool IsThereEncryptVer()  const { return (Flags & NHeader::NArchive::kEncryptVer) != 0; }
  // bool IsEncryptOld()       const { return (!IsThereEncryptVer() || EncryptVersion < 36); }

  bool AreMoreVolumes()       const { return (EndFlags & NHeader::NArchive::kEndOfArc_Flags_NextVol) != 0; }
  bool Is_VolNumber_Defined() const { return (EndFlags & NHeader::NArchive::kEndOfArc_Flags_VolNumber) != 0; }
  bool Is_DataCRC_Defined()   const { return (EndFlags & NHeader::NArchive::kEndOfArc_Flags_DataCRC) != 0; }
};

struct CArc
{
  CMyComPtr<IInStream> Stream;
  UInt64 PhySize;
  // CByteBuffer Comment;

  CArc(): PhySize(0) {}
  ISequentialInStream *CreateLimitedStream(UInt64 offset, UInt64 size) const;
};

struct CRefItem
{
  unsigned VolumeIndex;
  unsigned ItemIndex;
  unsigned NumItems;
};

class CHandler Z7_final:
  public IInArchive,
  Z7_PUBLIC_ISetCompressCodecsInfo_IFEC
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IInArchive)
  Z7_COM_QI_ENTRY_ISetCompressCodecsInfo_IFEC
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE
  
  Z7_IFACE_COM7_IMP(IInArchive)
  DECL_ISetCompressCodecsInfo

  CRecordVector<CRefItem> _refItems;
  CObjectVector<CItem> _items;
  CObjectVector<CArc> _arcs;
  NArchive::NRar::CInArcInfo _arcInfo;
  // AString _errorMessage;
  UInt32 _errorFlags;
  UInt32 _warningFlags;
  bool _isArc;
  UString _missingVolName;

  DECL_EXTERNAL_CODECS_VARS

  UInt64 GetPackSize(unsigned refIndex) const;
  bool IsSolid(unsigned refIndex) const;
  
  /*
  void AddErrorMessage(const AString &s)
  {
    if (!_errorMessage.IsEmpty())
      _errorMessage += '\n';
    _errorMessage += s;
  }
  */

  HRESULT Open2(IInStream *stream,
      const UInt64 *maxCheckStartPosition,
      IArchiveOpenCallback *openCallback);
};

}}

#endif
