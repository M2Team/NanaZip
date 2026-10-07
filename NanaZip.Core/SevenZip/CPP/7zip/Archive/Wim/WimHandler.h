// WimHandler.h

#ifndef ZIP7_INC_ARCHIVE_WIM_HANDLER_H
#define ZIP7_INC_ARCHIVE_WIM_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../Common/HandlerOut.h"

#include "WimIn.h"

namespace NArchive {
namespace NWim {

const Int32 kNumImagesMaxUpdate = 1 << 10;

Z7_CLASS_IMP_CHandler_IInArchive_5(
    IArchiveGetRawProps
  , IArchiveGetRootProps
  , IArchiveKeepModeForNextOpen
  , ISetProperties
  , IOutArchive
)
  bool _isOldVersion;
  bool _showImageNumber;
  bool _set_use_ShowImageNumber;
  bool _set_showImageNumber;

  CObjectVector<CVolume> _volumes;
  CDatabase _db;

  CObjectVector<CWimXml> _xmls;
  // unsigned _nameLenForStreams;
 
  bool _xmlInComments;

  bool _error_in_PartNumber;
  bool _volError;
  bool _xmlError;
  bool _isArc;
  bool _unsupported;

  bool _keepMode_ShowImageNumber;
  bool _disable_Sha1Check;

  bool _memAvail_wasSet;
  size_t _memAvail;

  unsigned _numXmlItems;
  unsigned _numIgnoreItems;
  int _defaultImageNumber;

  UInt32 _version;
  UInt32 _bootIndex;
  Int32 _firstVolumeIndex;
  unsigned _startingImageIndex;

  CHandlerTimeOptions _timeOptions;
  // UInt16 _temp_2bytes_NULL; // or UInt32
  UInt64 _phySize;

  void InitMemUseDefaults();

  void InitDefaults()
  {
    _memAvail_wasSet = false;
    _disable_Sha1Check = false;
    _set_use_ShowImageNumber = false;
    _set_showImageNumber = false;
    _defaultImageNumber = -1;
    _startingImageIndex = 1;
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

  void ClearErrors()
  {
    _error_in_PartNumber = false;
    _volError = false;
    _xmlError = false;
  }

  bool ThereIsError() const
  {
    return _error_in_PartNumber || _volError || _xmlError
      || _db.ThereIsError();
  }
  HRESULT GetSecurity(UInt32 realIndex, const void **data, UInt32 *dataSize, UInt32 *propType);

  HRESULT GetOutProperty(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, PROPVARIANT *value);
  HRESULT        GetTime(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, FILETIME &ft);
public:
  CHandler();
};

}}

#endif
