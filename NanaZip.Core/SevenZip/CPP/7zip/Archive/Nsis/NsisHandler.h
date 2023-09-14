// NSisHandler.h

#ifndef ZIP7_INC_NSIS_HANDLER_H
#define ZIP7_INC_NSIS_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../../Common/CreateCoder.h"

#include "../IArchive.h"

#include "NsisIn.h"

namespace NArchive {
namespace NNsis {

Z7_CLASS_IMP_CHandler_IInArchive_0

  CInArchive _archive;
  AString _methodString;

  bool GetUncompressedSize(unsigned index, UInt32 &size) const;
  bool GetCompressedSize(unsigned index, UInt32 &size) const;

  // AString GetMethod(NMethodType::EEnum method, bool useItemFilter, UInt32 dictionary) const;
};

}}

#endif
