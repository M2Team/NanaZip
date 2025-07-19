#include "../../../../C/CpuArch.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"

#include "../../../Windows/PropVariant.h"

#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"
#include "../../Common/StreamObjects.h"

#include "Header.h"

using namespace NWindows;
namespace NArchive {
  namespace NPyin {

    static const Byte kProps[] =
    {
      kpidPath,
      kpidSize,
    };

    IMP_IInArchive_Props
      IMP_IInArchive_ArcProps_NO_Table

      Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT* value))
    {
      COM_TRY_BEGIN
      NCOM::CPropVariant prop;
      switch (propID)
      {
      case kpidPhySize:
        prop = (UInt64)1;
        break;
      }
      prop.Detach(value);
      return S_OK;
      COM_TRY_END
    }

    Z7_COM7F_IMF(CHandler::Open(IInStream* stream, const UInt64*, IArchiveOpenCallback* callback)) {
      COM_TRY_BEGIN
      Close();
      if (!callback || !stream) {
        return S_FALSE;
      }
      HRESULT result = _archive.Open(stream, nullptr, callback);
      if (FAILED(result)) {
        return result;
      }

      _archive.items = _archive.items;

      UInt64 fileSize = 0;
      result = stream->Seek(0, STREAM_SEEK_END, &fileSize);
      if (FAILED(result) || fileSize == 0) {
        return S_FALSE;
      }
      stream->Seek(0, STREAM_SEEK_SET, nullptr);

      CMyComPtr<IArchiveOpenVolumeCallback> volumeCallback;
      result = callback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void**)&volumeCallback);
      if (FAILED(result) || !volumeCallback) {
        return S_FALSE;
      }

      UString name;
      {
        NCOM::CPropVariant prop;
        result = volumeCallback->GetProperty(kpidName, &prop);
        if (FAILED(result) || prop.vt != VT_BSTR) {
          return S_FALSE;
        }
        name = prop.bstrVal;
      }

      int dotPos = name.ReverseFind_Dot();
      if (dotPos == -1) {
        return S_FALSE;
      }

      const UString ext = name.Ptr(dotPos + 1);
      return StringsAreEqualNoCase_Ascii(ext, "exe") ? S_OK : S_FALSE;
      COM_TRY_END
    }


    Z7_COM7F_IMF(CHandler::Close()) {
      _archive.items.Clear();
      return S_OK;
    }

    Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32* numItems)) {
      if (numItems == nullptr) {
        return E_POINTER;
      }

      *numItems = static_cast<UInt32>(_archive.items.Size());

      return S_OK;
    }

    Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value))
    {
      COM_TRY_BEGIN
      NCOM::CPropVariant prop;
      switch (propID)
      {
      case kpidPath:
        prop = _archive.items[index].name; // implicit conversion works here
        break;
      case kpidSize: prop = _archive.items[index].uncompressedSize;
        break;
      }
      prop.Detach(value);
      return S_OK;
      COM_TRY_END

    }

    Z7_COM7F_IMF(CHandler::Extract(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback* extractCallback)) {
      COM_TRY_BEGIN
      bool allFilesMode = (numItems == (UInt32)(Int32)-1);
      if (allFilesMode) numItems = static_cast<UInt32>(_archive.items.Size());
      if (numItems == 0) return S_OK;

      UInt64 totalSize = 0, currentSize = 0;
      for (size_t i = 0; i < numItems; i++) {
        UInt32 index = allFilesMode ? i : indices[i];
        totalSize += _archive.items[index].uncompressedSize;
      }
      RINOK(extractCallback->SetTotal(totalSize));

      CLocalProgress* lps = new CLocalProgress;
      CMyComPtr<ICompressProgressInfo> progress = lps;
      lps->Init(extractCallback, false);

      for (UINT i = 0; i < numItems; i++) {
        lps->InSize = currentSize;
        lps->OutSize = currentSize;
        RINOK(lps->SetCur());

        CMyComPtr<ISequentialOutStream> realOutStream;
        Int32 askMode = testMode ? NExtract::NAskMode::kTest : NExtract::NAskMode::kExtract;
        UINT32 index = allFilesMode ? i : indices[i];
        auto& item = _archive.items[index];
        currentSize += item.uncompressedSize;

        RINOK(extractCallback->GetStream(index, &realOutStream, askMode));
        if (!testMode && !realOutStream) {
          continue;
        }

        RINOK(extractCallback->PrepareOperation(askMode));

        // Write to output stream
        HRESULT writeResult = realOutStream->Write(item.decompressedData, (UINT32)item.decompressedData.Size(), NULL);
        if (writeResult != S_OK) {
          return writeResult;
        }

        realOutStream.Release();
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      }

      lps->InSize = totalSize;
      lps->OutSize = totalSize;
      return lps->SetCur();
      COM_TRY_END

    }
  }
}