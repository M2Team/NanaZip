// ExtractMode.h

#ifndef __EXTRACT_MODE_H
#define __EXTRACT_MODE_H

namespace NExtract {
  
namespace NPathMode
{
  enum EEnum
  {
    kFullPaths,
    kCurPaths,
    kNoPaths,
    kAbsPaths,
    kNoPathsAlt // alt streams must be extracted without name of base file
  };
}

namespace NOverwriteMode
{
  enum EEnum
  {
    kAsk,
    kOverwrite,
    kSkip,
    kRename,
    kRenameExisting
  };
}

}

#endif
