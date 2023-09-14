// UdfHandler.h

#ifndef ZIP7_INC_UDF_HANDLER_H
#define ZIP7_INC_UDF_HANDLER_H

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

Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CRecordVector<CRef2> _refs2;
  CMyComPtr<IInStream> _inStream;
  CInArchive _archive;
};

}}

#endif
