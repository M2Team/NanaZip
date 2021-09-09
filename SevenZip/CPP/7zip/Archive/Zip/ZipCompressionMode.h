// CompressionMode.h

#ifndef __ZIP_COMPRESSION_MODE_H
#define __ZIP_COMPRESSION_MODE_H

#include "../../../Common/MyString.h"

#ifndef _7ZIP_ST
#include "../../../Windows/System.h"
#endif

#include "../Common/HandlerOut.h"

namespace NArchive {
namespace NZip {

const CMethodId kMethodId_ZipBase = 0x040100;
const CMethodId kMethodId_BZip2   = 0x040202;

struct CBaseProps: public CMultiMethodProps
{
  bool IsAesMode;
  Byte AesKeyMode;

  void Init()
  {
    CMultiMethodProps::Init();
    
    IsAesMode = false;
    AesKeyMode = 3;
  }
};

struct CCompressionMethodMode: public CBaseProps
{
  CRecordVector<Byte> MethodSequence;
  bool PasswordIsDefined;
  AString Password; // _Wipe

  UInt64 _dataSizeReduce;
  bool _dataSizeReduceDefined;
  
  bool IsRealAesMode() const { return PasswordIsDefined && IsAesMode; }

  CCompressionMethodMode(): PasswordIsDefined(false)
  {
    _dataSizeReduceDefined = false;
    _dataSizeReduce = 0;
  }

  ~CCompressionMethodMode() { Password.Wipe_and_Empty(); }
};

}}

#endif
