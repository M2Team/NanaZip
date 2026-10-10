#ifndef ZIP7_INC_ARCHIVE_PYINST_IN_H
#define ZIP7_INC_ARCHIVE_PYINST_IN_H

#include "../../../../C/CpuArch.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"


#include "../../../Windows/PropVariant.h"
#include "../../../Common/StringConvert.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"
#include "../../Archive/IArchive.h"
#include "../../Common/StreamObjects.h"

#include "StdAfx.h"
//#pragma comment(lib, "zlibstat.lib")

#ifdef _DEBUG
#include <windows.h>
#endif


class PyInstallerHandler {
private: // It's good practice to explicitly define access for private members
  UInt64 archiveSize;
  uint64_t overlayPos;

public:
  HRESULT Open(IInStream* stream, const UInt64*, IArchiveOpenCallback* callback);
  CMyComPtr<IArchiveOpenVolumeCallback> volCallback;
  HRESULT ParseArchive(IInStream* stream, UInt64 fileSize);
  HRESULT ReadData(IInStream* stream);

  // Consider making this static or a free function if it doesn't rely on class state
  uint32_t ByteSwap(uint32_t x)
  {
    return ((x >> 24) & 0x000000FF) |
           ((x >> 8) & 0x0000FF00) |
           ((x << 8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
  }

  struct ArchiveEntry
  {
      uint32_t entryPos;
      uint32_t compressedSize;
      uint32_t uncompressedSize;
      uint8_t compressionFlag;
      UString compressionType;
      UString name;
      CBuffer<uint8_t> data; // Holds raw compressed/stored data
      CBuffer<uint8_t> decompressedData; // Holds decompressed data
      UInt64 actualUncompressedSize; // Stores the actual size after decompression/copying

      // Constructor to initialize all members, including the new one
      ArchiveEntry(uint32_t entryPos, uint32_t compressedSize, uint32_t uncompressedSize,
          uint8_t compressionFlag, const UString& compressionType,
          const UString& name)
          : entryPos(entryPos),
          compressedSize(compressedSize),
          uncompressedSize(uncompressedSize),
          compressionFlag(compressionFlag),
          compressionType(compressionType),
          name(name),
          actualUncompressedSize(0) // Initialize to 0, it will be set later in ReadData
      {
      }
  };

  // Additional members for handling the PyInstaller archive
  UInt32 lengthofPackage = 0;             // Length of the package
  UInt32 toc = 0;                         // Table of contents
  UInt32 tocLen = 0;                      // Length of TOC
  UInt32 pyver = 0;                       // Python version
  uint64_t cookiePos = -1;                // Position of the cookie
  uint64_t tableOfContentsPos = 0;        // Position of the TOC
  uint64_t tableOfContentsSize = 0;       // Size of the TOC

  CObjectVector<ArchiveEntry> items;
};
#endif
