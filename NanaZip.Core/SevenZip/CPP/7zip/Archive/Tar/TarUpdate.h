// TarUpdate.h

#ifndef ZIP7_INC_TAR_UPDATE_H
#define ZIP7_INC_TAR_UPDATE_H

#include "../IArchive.h"

#include "TarItem.h"

namespace NArchive {
namespace NTar {

struct CUpdateItem
{
  int IndexInArc;
  unsigned IndexInClient;
  UInt64 Size;
  // Int64 MTime;
  UInt32 Mode;
  bool NewData;
  bool NewProps;
  bool IsDir;
  bool DeviceMajor_Defined;
  bool DeviceMinor_Defined;
  UInt32 UID;
  UInt32 GID;
  UInt32 DeviceMajor;
  UInt32 DeviceMinor;
  AString Name;
  AString User;
  AString Group;

  CPaxTimes PaxTimes;

  CUpdateItem():
      Size(0),
      IsDir(false),
      DeviceMajor_Defined(false),
      DeviceMinor_Defined(false),
      UID(0),
      GID(0)
      {}
};


struct CUpdateOptions
{
  UINT CodePage;
  unsigned UtfFlags;
  bool PosixMode;
  CBoolPair Write_MTime;
  CBoolPair Write_ATime;
  CBoolPair Write_CTime;
  CTimeOptions TimeOptions;
};


HRESULT UpdateArchive(IInStream *inStream, ISequentialOutStream *outStream,
    const CObjectVector<CItemEx> &inputItems,
    const CObjectVector<CUpdateItem> &updateItems,
    const CUpdateOptions &options,
    IArchiveUpdateCallback *updateCallback);

HRESULT GetPropString(IArchiveUpdateCallback *callback, UInt32 index, PROPID propId, AString &res,
    UINT codePage, unsigned utfFlags, bool convertSlash);

HRESULT Prop_To_PaxTime(const NWindows::NCOM::CPropVariant &prop, CPaxTime &pt);

void Get_AString_From_UString(const UString &s, AString &res,
    UINT codePage, unsigned utfFlags);

}}

#endif
