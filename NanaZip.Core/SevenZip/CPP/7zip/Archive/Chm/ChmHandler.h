// ChmHandler.h

#ifndef ZIP7_INC_ARCHIVE_CHM_HANDLER_H
#define ZIP7_INC_ARCHIVE_CHM_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../IArchive.h"

#include "ChmIn.h"

namespace NArchive {
namespace NChm {

Z7_CLASS_IMP_CHandler_IInArchive_0

  CFilesDatabase m_Database;
  CMyComPtr<IInStream> m_Stream;
  bool _help2;
  UInt32 m_ErrorFlags;
public:
  CHandler(bool help2): _help2(help2) {}
};

}}

#endif
