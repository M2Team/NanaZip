#include "../../../../C/CpuArch.h"
#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../Archive/IArchive.h"
#include "../../Common/StreamObjects.h"
#include "../../Compress/ZlibDecoder.h"

#include "PyInstallerRegister.h"
#include "PyInstallerHandler.h"
#include "StdAfx.h"
#include "BufInOutStream.h"


HRESULT SafeRead(IInStream* stream, void* buffer, size_t size, UInt32* bytesRead)
{
  HRESULT result = stream->Read(buffer, static_cast<UInt32>(size), bytesRead);
  if (result != S_OK || *bytesRead != size)
  {
    return S_FALSE;
  }
  return S_OK;
}

HRESULT PyInstallerHandler::Open(IInStream* stream, const UInt64*, IArchiveOpenCallback* callback) {
  if (!callback || !stream) {
    return S_FALSE; // Early return if inputs are invalid
  }

  // Query interface for volume callback
  HRESULT result = callback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void**)&volCallback);
  RINOK(result); // Use your error handling macro

  if (volCallback == nullptr) {
    return E_FAIL; // Return error if callback is unavailable
  }

  // Initialize and validate the file size
  UInt64 fileSize;
  RINOK(stream->Seek(0, STREAM_SEEK_END, &fileSize));

  const size_t searchChunkSize = 8192; // Define the chunk size for reading
  uint64_t endPos = fileSize;
  cookiePos = -1; // Initialize cookie position

  // Search for the magic number in reverse order
  while (true) {
    uint64_t startPos = (endPos >= searchChunkSize) ? endPos - searchChunkSize : 0;
    size_t chunkSize = endPos - startPos;
    if (chunkSize < kSignatureSize)
    {
      break;
    }
    RINOK(stream->Seek(startPos, STREAM_SEEK_SET, nullptr));
    CBuffer<char> data(chunkSize);
    UInt32 bytesRead = 0;
    RINOK(stream->Read(data, static_cast<UInt32>(chunkSize), &bytesRead));

    if (bytesRead != chunkSize) {
      return S_FALSE; // Return error if reading fails
    }

    for (Int64 i = static_cast<Int64>(chunkSize) - kSignatureSize; i >= 0; i--)
    {
      if (memcmp(data + i, kSignature, kSignatureSize) == 0)
      {
        cookiePos = startPos + i;
        break;
      }
    }

    endPos = startPos + kSignatureSize - 1;
    if (startPos == 0)
    {
      break;
    }

  }

  // Validate cookie position
  if (cookiePos == -1) {
    return S_FALSE; // Return if no cookie found
  }

  // Read version information
  RINOK(stream->Seek(cookiePos + 8, STREAM_SEEK_SET, nullptr));
  char versionBuffer[64];
  UInt32 bytesRead = 0;
  RINOK(stream->Read(versionBuffer, sizeof(versionBuffer), &bytesRead));

  if (bytesRead < sizeof(uint32_t)) {
    return S_FALSE; // Return if not enough data is read
  }

  // Determine PyInstaller version based on header contents
  uint32_t pyinstVer = 20;
  for (unsigned i = 0; i <= sizeof(versionBuffer) - 6; i++) {
    if (memcmp(versionBuffer + i, "python", 6) == 0) {
      pyinstVer = 21;
      break;
    }
  }
  // Read the main buffer for package information
  RINOK(stream->Seek(cookiePos, STREAM_SEEK_SET, nullptr));
  const size_t bufferSize = (pyinstVer == 20) ? 32 : 88; // Adjust based on version
  CBuffer<char> buffer(bufferSize);
  RINOK(stream->Read(buffer, bufferSize, &bytesRead));

  if (bytesRead != bufferSize) {
    return S_FALSE; // Return if buffer read fails
  }

  // Extract necessary fields from the buffer
  memcpy(&lengthofPackage, buffer + 8, 4);
  memcpy(&toc, buffer + 12, 4);
  memcpy(&tocLen, buffer + 16, 4);
  memcpy(&pyver, buffer + 20, 4);
  lengthofPackage = ByteSwap(lengthofPackage);
  toc = ByteSwap(toc);
  tocLen = ByteSwap(tocLen);
  pyver = ByteSwap(pyver);

  // Validate TOC and length values
  if (tocLen > fileSize || toc < 0 || lengthofPackage < 0) {
    return S_FALSE; // Return if invalid values are found
  }

  // Calculate overlay and TOC positions
  uint64_t tailBytes = fileSize - cookiePos - bufferSize;
  uint64_t overlaySize = static_cast<uint64_t>(lengthofPackage) + tailBytes;
  overlayPos = fileSize - overlaySize; // Store in member variable
  tableOfContentsPos = overlayPos + toc;
  tableOfContentsSize = tocLen;

  // Set total for callback
  RINOK(callback->SetTotal(nullptr, &tableOfContentsSize));

  // Parse the archive for entries
  HRESULT parseResult = ParseArchive(stream, fileSize);
  RINOK(parseResult);


  // Finalize archive size
  archiveSize = tableOfContentsSize + lengthofPackage;

  return S_OK; // Indicate success
}


HRESULT PyInstallerHandler::ParseArchive(IInStream* stream, UInt64 fileSize) {
  HRESULT result = stream->Seek(tableOfContentsPos, STREAM_SEEK_SET, nullptr);
  RINOK(result);

  uint32_t parsedLen = 0;
  while (parsedLen < tableOfContentsSize) {
    uint32_t entrySize;
    UInt32 bytesRead = 0;

    // Read entry size
    result = SafeRead(stream, &entrySize, sizeof(entrySize), &bytesRead);
    if (result != S_OK || bytesRead != sizeof(entrySize)) {
      return S_FALSE;
    }
    entrySize = ByteSwap(entrySize);

    if (entrySize > (tableOfContentsSize - parsedLen)) {
      return S_FALSE;
    }

    // Calculate name length and allocate buffer for the filename bytes
    uint32_t nameLen = sizeof(uint32_t) + sizeof(uint32_t) * 3 + sizeof(uint8_t) + sizeof(char);
    CBuffer<char> nameBuffer(entrySize - nameLen);

    uint32_t entryPos, compressedSize, uncompressedSize;
    uint8_t compressionFlag;
    char compressionType;

    // Read entry position
    result = SafeRead(stream, &entryPos, sizeof(entryPos), &bytesRead);
    if (result != S_OK || bytesRead != sizeof(entryPos)) {
      return S_FALSE;
    }
    entryPos = ByteSwap(entryPos);

    // Read compressed size
    result = SafeRead(stream, &compressedSize, sizeof(compressedSize), &bytesRead);
    if (result != S_OK || bytesRead != sizeof(compressedSize)) {
      return S_FALSE;
    }
    compressedSize = ByteSwap(compressedSize);

    // Read uncompressed size
    result = SafeRead(stream, &uncompressedSize, sizeof(uncompressedSize), &bytesRead);
    if (result != S_OK || bytesRead != sizeof(uncompressedSize)) {
      return S_FALSE;
    }
    uncompressedSize = ByteSwap(uncompressedSize);

    // Read compression flag
    result = SafeRead(stream, &compressionFlag, sizeof(compressionFlag), &bytesRead);
    if (result != S_OK || bytesRead != sizeof(compressionFlag)) {
      return S_FALSE;
    }

    // Read compression type
    result = SafeRead(stream, &compressionType, sizeof(compressionType), &bytesRead);
    if (result != S_OK || bytesRead != sizeof(compressionType)) {
      return S_FALSE;
    }

    // Read filename buffer
    result = SafeRead(stream, nameBuffer, nameBuffer.Size(), &bytesRead);
    if (result != S_OK || bytesRead != nameBuffer.Size()) {
      return S_FALSE;
    }

    AString utf8Name;
    utf8Name.SetFrom((const char*)nameBuffer, (unsigned)nameBuffer.Size());

    UString name;

    try {
      name = MultiByteToUnicodeString(utf8Name, CP_UTF8);

      for (int i = 0; i < name.Len(); ) {
        if (name[i] == L'\0')
          name.Delete(i);
        else
          ++i;
      }
    }
    catch (...) {
      UString fallback;
      try {
        fallback = MultiByteToUnicodeString(utf8Name, CP_UTF8);
      }
      catch (...) {
        fallback = L"<invalid utf8>";
      }

      name = L"random_";
      name.Add_UInt32((UInt32)rand());

      UString warn = L"[!] Warning: File name ";
      warn += fallback;
      warn += L" contains invalid bytes. Using random name ";
      warn += name;
    }

    // Normalize name
    if (!name.IsEmpty() && name[0] == L'/')
      name.Delete(0);

    if (name.IsEmpty()) {
      name = L"random_";
      name.Add_UInt32((UInt32)rand());
    }

    if (compressionType == 's' || compressionType == 'M' || compressionType == 'm')
      name += L".pyc";

    UInt64 actualOffset = overlayPos + entryPos;

    UString uType;
    uType += (wchar_t)compressionType;

    ArchiveEntry item(entryPos, compressedSize, uncompressedSize, compressionFlag, uType, name);
    items.Add(item);
    parsedLen += entrySize;
  }
  if (parsedLen != tableOfContentsSize) {
    return S_FALSE;
  }

  result = ReadData(stream);
  if (result != S_OK) {
    return result; // Return any error from ReadData
  }
  
  return S_OK;
}


HRESULT PyInstallerHandler::ReadData(IInStream* stream) {
  return S_OK;
}