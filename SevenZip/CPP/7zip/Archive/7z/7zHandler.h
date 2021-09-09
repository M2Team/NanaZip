// 7z/Handler.h

#ifndef __7Z_HANDLER_H
#define __7Z_HANDLER_H

#include "../../ICoder.h"
#include "../IArchive.h"

#include "../../Common/CreateCoder.h"

#ifndef __7Z_SET_PROPERTIES

#ifdef EXTRACT_ONLY
  #if !defined(_7ZIP_ST) && !defined(_SFX)
    #define __7Z_SET_PROPERTIES
  #endif
#else
  #define __7Z_SET_PROPERTIES
#endif

#endif

// #ifdef __7Z_SET_PROPERTIES
#include "../Common/HandlerOut.h"
// #endif

#include "7zCompressionMode.h"
#include "7zIn.h"

namespace NArchive {
namespace N7z {


#ifndef EXTRACT_ONLY

class COutHandler: public CMultiMethodProps
{
  HRESULT SetSolidFromString(const UString &s);
  HRESULT SetSolidFromPROPVARIANT(const PROPVARIANT &value);
public:
  UInt64 _numSolidFiles;
  UInt64 _numSolidBytes;
  bool _numSolidBytesDefined;
  bool _solidExtension;
  bool _useTypeSorting;

  bool _compressHeaders;
  bool _encryptHeadersSpecified;
  bool _encryptHeaders;
  // bool _useParents; 9.26

  CBoolPair Write_CTime;
  CBoolPair Write_ATime;
  CBoolPair Write_MTime;
  CBoolPair Write_Attrib;

  bool _useMultiThreadMixer;

  bool _removeSfxBlock;
  
  // bool _volumeMode;

  void InitSolidFiles() { _numSolidFiles = (UInt64)(Int64)(-1); }
  void InitSolidSize()  { _numSolidBytes = (UInt64)(Int64)(-1); }
  void InitSolid()
  {
    InitSolidFiles();
    InitSolidSize();
    _solidExtension = false;
    _numSolidBytesDefined = false;
  }

  void InitProps7z();
  void InitProps();

  COutHandler() { InitProps7z(); }

  HRESULT SetProperty(const wchar_t *name, const PROPVARIANT &value);
};

#endif

class CHandler:
  public IInArchive,
  public IArchiveGetRawProps,
  
  #ifdef __7Z_SET_PROPERTIES
  public ISetProperties,
  #endif
  
  #ifndef EXTRACT_ONLY
  public IOutArchive,
  #endif
  
  PUBLIC_ISetCompressCodecsInfo
  
  public CMyUnknownImp,

  #ifndef EXTRACT_ONLY
    public COutHandler
  #else
    public CCommonMethodProps
  #endif
{
public:
  MY_QUERYINTERFACE_BEGIN2(IInArchive)
  MY_QUERYINTERFACE_ENTRY(IArchiveGetRawProps)
  #ifdef __7Z_SET_PROPERTIES
  MY_QUERYINTERFACE_ENTRY(ISetProperties)
  #endif
  #ifndef EXTRACT_ONLY
  MY_QUERYINTERFACE_ENTRY(IOutArchive)
  #endif
  QUERY_ENTRY_ISetCompressCodecsInfo
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IInArchive(;)
  INTERFACE_IArchiveGetRawProps(;)

  #ifdef __7Z_SET_PROPERTIES
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
  #endif

  #ifndef EXTRACT_ONLY
  INTERFACE_IOutArchive(;)
  #endif

  DECL_ISetCompressCodecsInfo

  CHandler();
  ~CHandler()
  {
    Close();
  }

private:
  CMyComPtr<IInStream> _inStream;
  NArchive::N7z::CDbEx _db;
  
  #ifndef _NO_CRYPTO
  bool _isEncrypted;
  bool _passwordIsDefined;
  UString _password; // _Wipe
  #endif

  #ifdef EXTRACT_ONLY
  
  #ifdef __7Z_SET_PROPERTIES
  bool _useMultiThreadMixer;
  #endif

  UInt32 _crcSize;

  #else
  
  CRecordVector<CBond2> _bonds;

  HRESULT PropsMethod_To_FullMethod(CMethodFull &dest, const COneMethodInfo &m);
  HRESULT SetHeaderMethod(CCompressionMethodMode &headerMethod);
  HRESULT SetMainMethod(CCompressionMethodMode &method
      #ifndef _7ZIP_ST
      , UInt32 numThreads
      #endif
      );


  #endif

  bool IsFolderEncrypted(CNum folderIndex) const;
  #ifndef _SFX

  CRecordVector<UInt64> _fileInfoPopIDs;
  void FillPopIDs();
  void AddMethodName(AString &s, UInt64 id);
  HRESULT SetMethodToProp(CNum folderIndex, PROPVARIANT *prop) const;

  #endif

  DECL_EXTERNAL_CODECS_VARS
};

}}

#endif
