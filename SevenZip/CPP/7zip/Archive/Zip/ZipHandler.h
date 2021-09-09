// Zip/Handler.h

#ifndef __ZIP_HANDLER_H
#define __ZIP_HANDLER_H

#include "../../../Common/DynamicBuffer.h"
#include "../../ICoder.h"
#include "../IArchive.h"

#include "../../Common/CreateCoder.h"

#include "ZipCompressionMode.h"
#include "ZipIn.h"

namespace NArchive {
namespace NZip {

const unsigned kNumMethodNames1 = NFileHeader::NCompressionMethod::kZstdPk + 1;
const unsigned kMethodNames2Start = NFileHeader::NCompressionMethod::kZstdWz;
const unsigned kNumMethodNames2 = NFileHeader::NCompressionMethod::kWzAES + 1 - kMethodNames2Start;

extern const char * const kMethodNames1[kNumMethodNames1];
extern const char * const kMethodNames2[kNumMethodNames2];


class CHandler:
  public IInArchive,
  // public IArchiveGetRawProps,
  public IOutArchive,
  public ISetProperties,
  PUBLIC_ISetCompressCodecsInfo
  public CMyUnknownImp
{
public:
  MY_QUERYINTERFACE_BEGIN2(IInArchive)
  // MY_QUERYINTERFACE_ENTRY(IArchiveGetRawProps)
  MY_QUERYINTERFACE_ENTRY(IOutArchive)
  MY_QUERYINTERFACE_ENTRY(ISetProperties)
  QUERY_ENTRY_ISetCompressCodecsInfo
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IInArchive(;)
  // INTERFACE_IArchiveGetRawProps(;)
  INTERFACE_IOutArchive(;)

  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);

  DECL_ISetCompressCodecsInfo

  CHandler();
private:
  CObjectVector<CItemEx> m_Items;
  CInArchive m_Archive;

  CBaseProps _props;

  int m_MainMethod;
  bool m_ForceAesMode;
  bool m_WriteNtfsTimeExtra;
  bool _removeSfxBlock;
  bool m_ForceLocal;
  bool m_ForceUtf8;
  bool _forceCodePage;
  UInt32 _specifiedCodePage;

  DECL_EXTERNAL_CODECS_VARS

  void InitMethodProps()
  {
    _props.Init();
    m_MainMethod = -1;
    m_ForceAesMode = false;
    m_WriteNtfsTimeExtra = true;
    _removeSfxBlock = false;
    m_ForceLocal = false;
    m_ForceUtf8 = false;
    _forceCodePage = false;
    _specifiedCodePage = CP_OEMCP;
  }

  // void MarkAltStreams(CObjectVector<CItemEx> &items);

  HRESULT GetOutProperty(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, PROPVARIANT *value);
};

}}

#endif
