// NSisHandler.h

#ifndef __NSIS_HANDLER_H
#define __NSIS_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../../Common/CreateCoder.h"

#include "../IArchive.h"

#include "NsisIn.h"

namespace NArchive {
namespace NNsis {

class CHandler:
  public IInArchive,
  public CMyUnknownImp
{
  CInArchive _archive;
  AString _methodString;

  bool GetUncompressedSize(unsigned index, UInt32 &size) const;
  bool GetCompressedSize(unsigned index, UInt32 &size) const;

  // AString GetMethod(NMethodType::EEnum method, bool useItemFilter, UInt32 dictionary) const;
public:
  MY_UNKNOWN_IMP1(IInArchive)

  INTERFACE_IInArchive(;)
};

}}

#endif
