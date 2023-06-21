// NSisHandler.h

#ifndef __NSIS_HANDLER_H
#define __NSIS_HANDLER_H

#include "../../../../../ThirdParty/LZMA/CPP/Common/MyCom.h"

#include "../../../../../ThirdParty/LZMA/CPP/7zip/Common/CreateCoder.h"

#include "../../../../../ThirdParty/LZMA/CPP/7zip/Archive/IArchive.h"

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
