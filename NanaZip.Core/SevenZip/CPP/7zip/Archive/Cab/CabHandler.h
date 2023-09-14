// CabHandler.h

#ifndef ZIP7_INC_CAB_HANDLER_H
#define ZIP7_INC_CAB_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../IArchive.h"

#include "CabIn.h"

namespace NArchive {
namespace NCab {

Z7_CLASS_IMP_CHandler_IInArchive_0

  CMvDatabaseEx m_Database;
  UString _errorMessage;
  bool _isArc;
  bool _errorInHeaders;
  bool _unexpectedEnd;
  // int _mainVolIndex;
  UInt32 _phySize;
  UInt64 _offset;
};

}}

#endif
