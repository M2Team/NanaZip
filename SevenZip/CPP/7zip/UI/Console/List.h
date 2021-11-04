// List.h

#ifndef __LIST_H
#define __LIST_H

#include "../../../Common/Wildcard.h"

#include "../Common/LoadCodecs.h"

struct CListOptions
{
  bool ExcludeDirItems;
  bool ExcludeFileItems;

  CListOptions():
    ExcludeDirItems(false),
    ExcludeFileItems(false)
    {}
};

HRESULT ListArchives(
    const CListOptions &listOptions,
    CCodecs *codecs,
    const CObjectVector<COpenType> &types,
    const CIntVector &excludedFormats,
    bool stdInMode,
    UStringVector &archivePaths, UStringVector &archivePathsFull,
    bool processAltStreams, bool showAltStreams,
    const NWildcard::CCensorNode &wildcardCensor,
    bool enableHeaders, bool techMode,
  #ifndef _NO_CRYPTO
    bool &passwordEnabled, UString &password,
  #endif
  #ifndef _SFX
    const CObjectVector<CProperty> *props,
  #endif
    UInt64 &errors,
    UInt64 &numWarnings);

#endif
