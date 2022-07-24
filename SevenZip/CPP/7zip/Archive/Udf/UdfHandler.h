// UdfHandler.h

#ifndef __UDF_HANDLER_H
#define __UDF_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../IArchive.h"

#include "UdfIn.h"

namespace NArchive {
namespace NUdf {

struct CRef2
{
  unsigned Vol;
  unsigned Fs;
  unsigned Ref;
};

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CRecordVector<CRef2> _refs2;
  CMyComPtr<IInStream> _inStream;
  CInArchive _archive;
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

}}

#endif
