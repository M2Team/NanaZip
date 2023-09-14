// WimHandler.h

#ifndef ZIP7_INC_ARCHIVE_WIM_HANDLER_H
#define ZIP7_INC_ARCHIVE_WIM_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../Common/HandlerOut.h"

#include "WimIn.h"

namespace NArchive {
namespace NWim {

static const Int32 kNumImagesMaxUpdate = (1 << 10);

Z7_CLASS_IMP_CHandler_IInArchive_5(
    IArchiveGetRawProps
  , IArchiveGetRootProps
  , IArchiveKeepModeForNextOpen
  , ISetProperties
  , IOutArchive
)
  CDatabase _db;
  UInt32 _version;
  bool _isOldVersion;
  UInt32 _bootIndex;

  CObjectVector<CVolume> _volumes;
  CObjectVector<CWimXml> _xmls;
  // unsigned _nameLenForStreams;
  bool _xmlInComments;
  
  unsigned _numXmlItems;
  unsigned _numIgnoreItems;

  bool _xmlError;
  bool _isArc;
  bool _unsupported;

  bool _set_use_ShowImageNumber;
  bool _set_showImageNumber;
  int _defaultImageNumber;

  bool _showImageNumber;

  bool _keepMode_ShowImageNumber;

  UInt64 _phySize;
  int _firstVolumeIndex;

  CHandlerTimeOptions _timeOptions;

  void InitDefaults()
  {
    _set_use_ShowImageNumber = false;
    _set_showImageNumber = false;
    _defaultImageNumber = -1;
    _timeOptions.Init();
  }

  bool IsUpdateSupported() const
  {
    if (ThereIsError()) return false;
    if (_db.Images.Size() > kNumImagesMaxUpdate) return false;

    // Solid format is complicated. So we disable updating now.
    if (!_db.Solids.IsEmpty()) return false;

    if (_volumes.Size() == 0)
      return true;
    
    if (_volumes.Size() != 2) return false;
    if (_volumes[0].Stream) return false;
    if (_version != k_Version_NonSolid
        // && _version != k_Version_Solid
        ) return false;
    
    return true;
  }

  bool ThereIsError() const { return _xmlError || _db.ThereIsError(); }
  HRESULT GetSecurity(UInt32 realIndex, const void **data, UInt32 *dataSize, UInt32 *propType);

  HRESULT GetOutProperty(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, PROPVARIANT *value);
  HRESULT        GetTime(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, FILETIME &ft);
public:
  CHandler();
};

}}

#endif
