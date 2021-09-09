// ExtractEngine.h

#ifndef __EXTRACT_ENGINE_H
#define __EXTRACT_ENGINE_H

#include "../../UI/Common/LoadCodecs.h"

HRESULT ExtractArchive(CCodecs *codecs, const FString &fileName, const FString &destFolder,
    bool showProgress, bool &isCorrupt, UString &errorMessage);

#endif
