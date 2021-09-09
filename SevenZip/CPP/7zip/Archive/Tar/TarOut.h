// Archive/TarOut.h

#ifndef __ARCHIVE_TAR_OUT_H
#define __ARCHIVE_TAR_OUT_H

#include "../../../Common/MyCom.h"

#include "../../IStream.h"

#include "TarItem.h"

namespace NArchive {
namespace NTar {

class COutArchive
{
  CMyComPtr<ISequentialOutStream> m_Stream;

  HRESULT WriteBytes(const void *data, unsigned size);
  HRESULT WriteHeaderReal(const CItem &item);
public:
  UInt64 Pos;
  
  void Create(ISequentialOutStream *outStream)
  {
    m_Stream = outStream;
  }

  HRESULT WriteHeader(const CItem &item);
  HRESULT FillDataResidual(UInt64 dataSize);
  HRESULT WriteFinishHeader();
};

}}

#endif
