// TempFiles.h

#ifndef __TEMP_FILES_H
#define __TEMP_FILES_H

#include "../../../../../ThirdParty/LZMA/CPP/Common/MyString.h"

class CTempFiles
{
  void Clear();
public:
  FStringVector Paths;
  ~CTempFiles() { Clear(); }
};

#endif
