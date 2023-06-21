// EnumFormatEtc.h

#ifndef __ENUMFORMATETC_H
#define __ENUMFORMATETC_H

#include "../../../../../ThirdParty/LZMA/CPP/Common/MyWindows.h"

HRESULT CreateEnumFormatEtc(UINT numFormats, const FORMATETC *formats, IEnumFORMATETC **enumFormat);

#endif
