// Common/Exception.h

#ifndef __COMMON_EXCEPTION_H
#define __COMMON_EXCEPTION_H

#include "MyWindows.h"

struct CSystemException
{
  HRESULT ErrorCode;
  CSystemException(HRESULT errorCode): ErrorCode(errorCode) {}
};

#endif
