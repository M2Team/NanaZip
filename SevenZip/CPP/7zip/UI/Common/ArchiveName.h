// ArchiveName.h

#ifndef __ARCHIVE_NAME_H
#define __ARCHIVE_NAME_H

#include "../../../Windows/FileFind.h"

UString CreateArchiveName(const UStringVector &paths, const NWindows::NFile::NFind::CFileInfo *fi = NULL);

#endif
