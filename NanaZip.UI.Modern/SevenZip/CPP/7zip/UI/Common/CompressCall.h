// CompressCall.h

#ifndef __COMPRESS_CALL_H
#define __COMPRESS_CALL_H

#include "../../../Common/MyString.h"

UString GetQuotedString(const UString &s);

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish);

// **************** NanaZip Modification Start ****************
// void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone);
void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup, UInt32 writeZone, bool smartExtract = false);
// **************** NanaZip Modification End ****************
void TestArchives(const UStringVector &arcPaths, bool hashMode = false);

void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName);

void Benchmark(bool totalMode);

#endif
