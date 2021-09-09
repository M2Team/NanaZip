// ChmHandler.h

#ifndef __ARCHIVE_CHM_HANDLER_H
#define __ARCHIVE_CHM_HANDLER_H

#include "../../../Common/MyCom.h"

#include "../IArchive.h"

#include "ChmIn.h"

namespace NArchive {
namespace NChm {

class CHandler:
  public IInArchive,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(IInArchive)

  INTERFACE_IInArchive(;)

  bool _help2;
  CHandler(bool help2): _help2(help2) {}

private:
  CFilesDatabase m_Database;
  CMyComPtr<IInStream> m_Stream;
  UInt32 m_ErrorFlags;
};

}}

#endif
