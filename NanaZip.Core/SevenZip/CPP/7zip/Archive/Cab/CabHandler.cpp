// CabHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../../C/Alloc.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"

#include "../../Compress/CopyCoder.h"
#include "../../Compress/DeflateDecoder.h"
#include "../../Compress/LzxDecoder.h"
#include "../../Compress/QuantumDecoder.h"

#include "../Common/ItemNameUtils.h"

#include "CabBlockInStream.h"
#include "CabHandler.h"

using namespace NWindows;

namespace NArchive {
namespace NCab {

// #define CAB_DETAILS

#ifdef CAB_DETAILS
enum
{
  kpidBlockReal = kpidUserDefined
};
#endif

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidMTime,
  kpidAttrib,
  kpidMethod,
  kpidBlock
  #ifdef CAB_DETAILS
  ,
  // kpidBlockReal, // L"BlockReal",
  kpidOffset,
  kpidVolume
  #endif
};

static const Byte kArcProps[] =
{
  kpidTotalPhySize,
  kpidMethod,
  // kpidSolid,
  kpidNumBlocks,
  kpidNumVolumes,
  kpidVolumeIndex,
  kpidId
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static const char * const kMethods[] =
{
    "None"
  , "MSZip"
  , "Quantum"
  , "LZX"
};

static const unsigned kMethodNameBufSize = 32; // "Quantum:255"

static void SetMethodName(char *s, unsigned method, unsigned param)
{
  if (method < Z7_ARRAY_SIZE(kMethods))
  {
    s = MyStpCpy(s, kMethods[method]);
    if (method != NHeader::NMethod::kLZX &&
        method != NHeader::NMethod::kQuantum)
      return;
    *s++ = ':';
    method = param;
  }
  ConvertUInt32ToString(method, s);
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMethod:
    {
      UInt32 mask = 0;
      UInt32 params[2] = { 0, 0 };
      {
        FOR_VECTOR (v, m_Database.Volumes)
        {
          const CRecordVector<CFolder> &folders = m_Database.Volumes[v].Folders;
          FOR_VECTOR (i, folders)
          {
            const CFolder &folder = folders[i];
            unsigned method = folder.GetMethod();
            mask |= ((UInt32)1 << method);
            if (method == NHeader::NMethod::kLZX ||
                method == NHeader::NMethod::kQuantum)
            {
              unsigned di = (method == NHeader::NMethod::kQuantum) ? 0 : 1;
              if (params[di] < folder.MethodMinor)
                params[di] = folder.MethodMinor;
            }
          }
        }
      }
      
      AString s;
      
      for (unsigned i = 0; i < kNumMethodsMax; i++)
      {
        if ((mask & (1 << i)) == 0)
          continue;
        s.Add_Space_if_NotEmpty();
        char temp[kMethodNameBufSize];
        SetMethodName(temp, i, params[i == NHeader::NMethod::kQuantum ? 0 : 1]);
        s += temp;
      }
      
      prop = s;
      break;
    }
    // case kpidSolid: prop = _database.IsSolid(); break;
    case kpidNumBlocks:
    {
      UInt32 numFolders = 0;
      FOR_VECTOR (v, m_Database.Volumes)
        numFolders += m_Database.Volumes[v].Folders.Size();
      prop = numFolders;
      break;
    }

    case kpidTotalPhySize:
    {
      if (m_Database.Volumes.Size() > 1)
      {
        UInt64 sum = 0;
        FOR_VECTOR (v, m_Database.Volumes)
          sum += m_Database.Volumes[v].ArcInfo.Size;
        prop = sum;
      }
      break;
    }

    case kpidNumVolumes:
      prop = (UInt32)m_Database.Volumes.Size();
      break;

    case kpidVolumeIndex:
    {
      if (!m_Database.Volumes.IsEmpty())
      {
        const CDatabaseEx &db = m_Database.Volumes[0];
        const CInArcInfo &ai = db.ArcInfo;
        prop = (UInt32)ai.CabinetNumber;
      }
      break;
    }

    case kpidId:
    {
      if (m_Database.Volumes.Size() != 0)
      {
        prop = (UInt32)m_Database.Volumes[0].ArcInfo.SetID;
      }
      break;
    }

    case kpidOffset:
      /*
      if (m_Database.Volumes.Size() == 1)
        prop = m_Database.Volumes[0].StartPosition;
      */
      prop = _offset;
      break;
    
    case kpidPhySize:
      /*
      if (m_Database.Volumes.Size() == 1)
        prop = (UInt64)m_Database.Volumes[0].ArcInfo.Size;
      */
      prop = (UInt64)_phySize;
      break;

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_errorInHeaders) v |= kpv_ErrorFlags_HeadersError;
      if (_unexpectedEnd)  v |= kpv_ErrorFlags_UnexpectedEnd;
      prop = v;
      break;
    }
    
    case kpidError:
      if (!_errorMessage.IsEmpty())
        prop = _errorMessage;
      break;

    case kpidName:
    {
      if (m_Database.Volumes.Size() == 1)
      {
        const CDatabaseEx &db = m_Database.Volumes[0];
        const CInArcInfo &ai = db.ArcInfo;
        if (ai.SetID != 0)
        {
          AString s;
          s.Add_UInt32(ai.SetID);
          s += '_';
          s.Add_UInt32(ai.CabinetNumber + 1);
          s += ".cab";
          prop = s;
        }
        /*
        // that code is incomplete. It gcan give accurate name of volume
        char s[32];
        ConvertUInt32ToString(ai.CabinetNumber + 2, s);
        unsigned len = MyStringLen(s);
        if (ai.IsThereNext())
        {
          AString fn = ai.NextArc.FileName;
          if (fn.Len() > 4 && StringsAreEqualNoCase_Ascii(fn.RightPtr(4), ".cab"))
            fn.DeleteFrom(fn.Len() - 4);
          if (len < fn.Len())
          {
            if (strcmp(s, fn.RightPtr(len)) == 0)
            {
              AString s2 = fn;
              s2.DeleteFrom(fn.Len() - len);
              ConvertUInt32ToString(ai.CabinetNumber + 1, s);
              s2 += s;
              s2 += ".cab";
              prop = GetUnicodeString(s2);
            }
          }
        }
        */
      }
      break;
    }

    // case kpidShortComment:
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  
  const CMvItem &mvItem = m_Database.Items[index];
  const CDatabaseEx &db = m_Database.Volumes[mvItem.VolumeIndex];
  unsigned itemIndex = mvItem.ItemIndex;
  const CItem &item = db.Items[itemIndex];
  switch (propID)
  {
    case kpidPath:
    {
      UString unicodeName;
      if (item.IsNameUTF())
        ConvertUTF8ToUnicode(item.Name, unicodeName);
      else
        unicodeName = MultiByteToUnicodeString(item.Name, CP_ACP);
      prop = (const wchar_t *)NItemName::WinPathToOsPath(unicodeName);
      break;
    }
    
    case kpidIsDir:  prop = item.IsDir(); break;
    case kpidSize:  prop = item.Size; break;
    case kpidAttrib:  prop = item.GetWinAttrib(); break;

    case kpidMTime:
    {
      PropVariant_SetFrom_DosTime(prop, item.Time);
      break;
    }

    case kpidMethod:
    {
      const int realFolderIndex = item.GetFolderIndex(db.Folders.Size());
      if (realFolderIndex >= 0)
      {
        const CFolder &folder = db.Folders[(unsigned)realFolderIndex];
        char s[kMethodNameBufSize];
        SetMethodName(s, folder.GetMethod(), folder.MethodMinor);
        prop = s;
      }
      break;
    }

    case kpidBlock:  prop.Set_Int32((Int32)m_Database.GetFolderIndex(&mvItem)); break;
    
    #ifdef CAB_DETAILS
    
    // case kpidBlockReal:  prop = (UInt32)item.FolderIndex; break;
    case kpidOffset:  prop = (UInt32)item.Offset; break;
    case kpidVolume:  prop = (UInt32)mvItem.VolumeIndex; break;

    #endif
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 *maxCheckStartPosition,
    IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  Close();

  CInArchive archive;
  CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
  if (callback)
    callback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback);
  
  CMyComPtr<IInStream> nextStream = inStream;
  bool prevChecked = false;
  UString startVolName;
  bool startVolName_was_Requested = false;
  UInt64 numItems = 0;
  unsigned numTempVolumes = 0;
  // try
  {
    while (nextStream)
    {
      CDatabaseEx db;
      db.Stream = nextStream;
      
      HRESULT res = archive.Open(db, maxCheckStartPosition);
      
      _errorInHeaders |= archive.HeaderError;
      _errorInHeaders |= archive.ErrorInNames;
      _unexpectedEnd |= archive.UnexpectedEnd;
      
      if (res == S_OK && !m_Database.Volumes.IsEmpty())
      {
        const CArchInfo &lastArc = m_Database.Volumes.Back().ArcInfo;
        unsigned cabNumber = db.ArcInfo.CabinetNumber;
        if (lastArc.SetID != db.ArcInfo.SetID)
          res = S_FALSE;
        else if (prevChecked)
        {
          if (cabNumber != lastArc.CabinetNumber + 1)
            res = S_FALSE;
        }
        else if (cabNumber >= lastArc.CabinetNumber)
          res = S_FALSE;
        else if (numTempVolumes != 0)
        {
          const CArchInfo &prevArc = m_Database.Volumes[numTempVolumes - 1].ArcInfo;
          if (cabNumber != prevArc.CabinetNumber + 1)
            res = S_FALSE;
        }
      }
      
      if (archive.IsArc || res == S_OK)
      {
        _isArc = true;
        if (m_Database.Volumes.IsEmpty())
        {
          _offset = db.StartPosition;
          _phySize = db.ArcInfo.Size;
        }
      }
      
      if (res == S_OK)
      {
        numItems += db.Items.Size();
        m_Database.Volumes.Insert(prevChecked ? m_Database.Volumes.Size() : numTempVolumes, db);
        if (!prevChecked && m_Database.Volumes.Size() > 1)
        {
          numTempVolumes++;
          if (db.ArcInfo.CabinetNumber + 1 == m_Database.Volumes[numTempVolumes].ArcInfo.CabinetNumber)
            numTempVolumes = 0;
        }
      }
      else
      {
        if (res != S_FALSE)
          return res;
        if (m_Database.Volumes.IsEmpty())
          return S_FALSE;
        if (prevChecked)
          break;
        prevChecked = true;
        if (numTempVolumes != 0)
        {
          m_Database.Volumes.DeleteFrontal(numTempVolumes);
          numTempVolumes = 0;
        }
      }

      if (callback)
      {
        RINOK(callback->SetCompleted(&numItems, NULL))
      }
        
      nextStream = NULL;
      
      for (;;)
      {
        const COtherArc *otherArc = NULL;
        
        if (!prevChecked)
        {
          if (numTempVolumes == 0)
          {
            const CInArcInfo &ai = m_Database.Volumes[0].ArcInfo;
            if (ai.IsTherePrev())
              otherArc = &ai.PrevArc;
            else
              prevChecked = true;
          }
          else
          {
            const CInArcInfo &ai = m_Database.Volumes[numTempVolumes - 1].ArcInfo;
            if (ai.IsThereNext())
              otherArc = &ai.NextArc;
            else
            {
              prevChecked = true;
              m_Database.Volumes.DeleteFrontal(numTempVolumes);
              numTempVolumes = 0;
            }
          }
        }
        
        if (!otherArc)
        {
          const CInArcInfo &ai = m_Database.Volumes.Back().ArcInfo;
          if (ai.IsThereNext())
            otherArc = &ai.NextArc;
        }
        
        if (!otherArc)
          break;
        if (!openVolumeCallback)
          break;
        // printf("\n%s", otherArc->FileName);
        const UString fullName = MultiByteToUnicodeString(otherArc->FileName, CP_ACP);

        if (!startVolName_was_Requested)
        {
          // some "bad" cab example can contain the link to itself.
          startVolName_was_Requested = true;
          {
            NCOM::CPropVariant prop;
            RINOK(openVolumeCallback->GetProperty(kpidName, &prop))
            if (prop.vt == VT_BSTR)
              startVolName = prop.bstrVal;
          }
          if (fullName == startVolName)
            break;
        }

        HRESULT result = openVolumeCallback->GetStream(fullName, &nextStream);
        if (result == S_OK)
          break;
        if (result != S_FALSE)
          return result;

        if (!_errorMessage.IsEmpty())
          _errorMessage.Add_LF();
        _errorMessage += "Can't open volume: ";
        _errorMessage += fullName;
        
        if (prevChecked)
          break;
        prevChecked = true;
        if (numTempVolumes != 0)
        {
          m_Database.Volumes.DeleteFrontal(numTempVolumes);
          numTempVolumes = 0;
        }
      }

    } // read nextStream iteration

    if (numTempVolumes != 0)
    {
      m_Database.Volumes.DeleteFrontal(numTempVolumes);
      numTempVolumes = 0;
    }
    if (m_Database.Volumes.IsEmpty())
      return S_FALSE;
    else
    {
      m_Database.FillSortAndShrink();
      if (!m_Database.Check())
        return S_FALSE;
    }
  }
  COM_TRY_END
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Close())
{
  _errorMessage.Empty();
  _isArc = false;
  _errorInHeaders = false;
  _unexpectedEnd = false;
  // _mainVolIndex = -1;
  _phySize = 0;
  _offset = 0;

  m_Database.Clear();
  return S_OK;
}


Z7_CLASS_IMP_NOQIB_1(
  CFolderOutStream
  , ISequentialOutStream
)
  const CMvDatabaseEx *m_Database;
  const CRecordVector<bool> *m_ExtractStatuses;
  
  Byte *TempBuf;
  UInt32 TempBufSize;
  UInt32 TempBufWritten;
  unsigned NumIdenticalFiles;
  bool TempBufMode;

  unsigned m_StartIndex;
  unsigned m_CurrentIndex;
  CMyComPtr<IArchiveExtractCallback> m_ExtractCallback;
  bool m_TestMode;

  CMyComPtr<ISequentialOutStream> m_RealOutStream;

  bool m_IsOk;
  bool m_FileIsOpen;
  UInt32 m_RemainFileSize;
  UInt64 m_FolderSize;
  UInt64 m_PosInFolder;

  void FreeTempBuf()
  {
    ::MyFree(TempBuf);
    TempBuf = NULL;
  }

  HRESULT OpenFile();
  HRESULT CloseFileWithResOp(Int32 resOp);
  HRESULT CloseFile();
public:
  HRESULT WriteEmptyFiles();

  CFolderOutStream(): TempBuf(NULL) {}
  ~CFolderOutStream() { FreeTempBuf(); }
  void Init(
      const CMvDatabaseEx *database,
      const CRecordVector<bool> *extractStatuses,
      unsigned startIndex,
      UInt64 folderSize,
      IArchiveExtractCallback *extractCallback,
      bool testMode);
  HRESULT FlushCorrupted(unsigned folderIndex);
  HRESULT Unsupported();

  bool NeedMoreWrite() const { return (m_FolderSize > m_PosInFolder); }
  UInt64 GetRemain() const { return m_FolderSize - m_PosInFolder; }
  UInt64 GetPosInFolder() const { return m_PosInFolder; }
};


void CFolderOutStream::Init(
    const CMvDatabaseEx *database,
    const CRecordVector<bool> *extractStatuses,
    unsigned startIndex,
    UInt64 folderSize,
    IArchiveExtractCallback *extractCallback,
    bool testMode)
{
  m_Database = database;
  m_ExtractStatuses = extractStatuses;
  m_StartIndex = startIndex;
  m_FolderSize = folderSize;

  m_ExtractCallback = extractCallback;
  m_TestMode = testMode;

  m_CurrentIndex = 0;
  m_PosInFolder = 0;
  m_FileIsOpen = false;
  m_IsOk = true;
  TempBufMode = false;
  NumIdenticalFiles = 0;
}


HRESULT CFolderOutStream::CloseFileWithResOp(Int32 resOp)
{
  m_RealOutStream.Release();
  m_FileIsOpen = false;
  NumIdenticalFiles--;
  return m_ExtractCallback->SetOperationResult(resOp);
}


HRESULT CFolderOutStream::CloseFile()
{
  return CloseFileWithResOp(m_IsOk ?
      NExtract::NOperationResult::kOK:
      NExtract::NOperationResult::kDataError);
}


HRESULT CFolderOutStream::OpenFile()
{
  if (NumIdenticalFiles == 0)
  {
    const CMvItem &mvItem = m_Database->Items[m_StartIndex + m_CurrentIndex];
    const CItem &item = m_Database->Volumes[mvItem.VolumeIndex].Items[mvItem.ItemIndex];
    unsigned numExtractItems = 0;
    unsigned curIndex;
    
    for (curIndex = m_CurrentIndex; curIndex < m_ExtractStatuses->Size(); curIndex++)
    {
      const CMvItem &mvItem2 = m_Database->Items[m_StartIndex + curIndex];
      const CItem &item2 = m_Database->Volumes[mvItem2.VolumeIndex].Items[mvItem2.ItemIndex];
      if (item.Offset != item2.Offset ||
          item.Size != item2.Size ||
          item.Size == 0)
        break;
      if (!m_TestMode && (*m_ExtractStatuses)[curIndex])
        numExtractItems++;
    }
    
    NumIdenticalFiles = (curIndex - m_CurrentIndex);
    if (NumIdenticalFiles == 0)
      NumIdenticalFiles = 1;
    TempBufMode = false;
    
    if (numExtractItems > 1)
    {
      if (!TempBuf || item.Size > TempBufSize)
      {
        FreeTempBuf();
        TempBuf = (Byte *)MyAlloc(item.Size);
        TempBufSize = item.Size;
        if (!TempBuf)
          return E_OUTOFMEMORY;
      }
      TempBufMode = true;
      TempBufWritten = 0;
    }
    else if (numExtractItems == 1)
    {
      while (NumIdenticalFiles && !(*m_ExtractStatuses)[m_CurrentIndex])
      {
        CMyComPtr<ISequentialOutStream> stream;
        RINOK(m_ExtractCallback->GetStream(m_StartIndex + m_CurrentIndex, &stream, NExtract::NAskMode::kSkip))
        if (stream)
          return E_FAIL;
        RINOK(m_ExtractCallback->PrepareOperation(NExtract::NAskMode::kSkip))
        m_CurrentIndex++;
        m_FileIsOpen = true;
        CloseFile();
      }
    }
  }

  Int32 askMode = (*m_ExtractStatuses)[m_CurrentIndex] ? m_TestMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract :
      NExtract::NAskMode::kSkip;
  RINOK(m_ExtractCallback->GetStream(m_StartIndex + m_CurrentIndex, &m_RealOutStream, askMode))
  if (!m_RealOutStream && !m_TestMode)
    askMode = NExtract::NAskMode::kSkip;
  return m_ExtractCallback->PrepareOperation(askMode);
}


HRESULT CFolderOutStream::WriteEmptyFiles()
{
  if (m_FileIsOpen)
    return S_OK;
  for (; m_CurrentIndex < m_ExtractStatuses->Size(); m_CurrentIndex++)
  {
    const CMvItem &mvItem = m_Database->Items[m_StartIndex + m_CurrentIndex];
    const CItem &item = m_Database->Volumes[mvItem.VolumeIndex].Items[mvItem.ItemIndex];
    UInt64 fileSize = item.Size;
    if (fileSize != 0)
      return S_OK;
    HRESULT result = OpenFile();
    m_RealOutStream.Release();
    RINOK(result)
    RINOK(m_ExtractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
  }
  return S_OK;
}


Z7_COM7F_IMF(CFolderOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  // (data == NULL) means Error_Data for solid folder flushing
  COM_TRY_BEGIN
  
  UInt32 realProcessed = 0;
  if (processedSize)
   *processedSize = 0;
  
  while (size != 0)
  {
    if (m_FileIsOpen)
    {
      UInt32 numBytesToWrite = MyMin(m_RemainFileSize, size);
      HRESULT res = S_OK;
      if (numBytesToWrite != 0)
      {
        if (!data)
          m_IsOk = false;
        
        if (m_RealOutStream)
        {
          UInt32 processedSizeLocal = 0;
          // 18.01 : we don't want ZEROs instead of missing data
          if (data)
            res = m_RealOutStream->Write((const Byte *)data, numBytesToWrite, &processedSizeLocal);
          else
            processedSizeLocal = numBytesToWrite;
          numBytesToWrite = processedSizeLocal;
        }
        
        if (TempBufMode && TempBuf)
        {
          if (data)
          {
            memcpy(TempBuf + TempBufWritten, data, numBytesToWrite);
            TempBufWritten += numBytesToWrite;
          }
        }
      }
      realProcessed += numBytesToWrite;
      if (processedSize)
        *processedSize = realProcessed;
      if (data)
        data = (const void *)((const Byte *)data + numBytesToWrite);
      size -= numBytesToWrite;
      m_RemainFileSize -= numBytesToWrite;
      m_PosInFolder += numBytesToWrite;
      
      if (res != S_OK)
        return res;
      
      if (m_RemainFileSize == 0)
      {
        RINOK(CloseFile())

        while (NumIdenticalFiles)
        {
          HRESULT result = OpenFile();
          m_FileIsOpen = true;
          m_CurrentIndex++;
          if (result == S_OK && m_RealOutStream && TempBuf)
            result = WriteStream(m_RealOutStream, TempBuf, TempBufWritten);
          
          if (!TempBuf && TempBufMode && m_RealOutStream)
          {
            RINOK(CloseFileWithResOp(NExtract::NOperationResult::kUnsupportedMethod))
          }
          else
          {
            RINOK(CloseFile())
          }
      
          RINOK(result)
        }
      
        TempBufMode = false;
      }
  
      if (realProcessed > 0)
        break; // with this break this function works as Write-Part
    }
    else
    {
      if (m_CurrentIndex >= m_ExtractStatuses->Size())
      {
        // we ignore extra data;
        realProcessed += size;
        if (processedSize)
          *processedSize = realProcessed;
        m_PosInFolder += size;
        return S_OK;
        // return E_FAIL;
      }

      const CMvItem &mvItem = m_Database->Items[m_StartIndex + m_CurrentIndex];
      const CItem &item = m_Database->Volumes[mvItem.VolumeIndex].Items[mvItem.ItemIndex];

      m_RemainFileSize = item.Size;

      UInt32 fileOffset = item.Offset;
      
      if (fileOffset < m_PosInFolder)
        return E_FAIL;
      
      if (fileOffset > m_PosInFolder)
      {
        UInt32 numBytesToWrite = MyMin(fileOffset - (UInt32)m_PosInFolder, size);
        realProcessed += numBytesToWrite;
        if (processedSize)
          *processedSize = realProcessed;
        if (data)
          data = (const void *)((const Byte *)data + numBytesToWrite);
        size -= numBytesToWrite;
        m_PosInFolder += numBytesToWrite;
      }
      
      if (fileOffset == m_PosInFolder)
      {
        RINOK(OpenFile())
        m_FileIsOpen = true;
        m_CurrentIndex++;
        m_IsOk = true;
      }
    }
  }
  
  return WriteEmptyFiles();
  
  COM_TRY_END
}


HRESULT CFolderOutStream::FlushCorrupted(unsigned folderIndex)
{
  if (!NeedMoreWrite())
  {
    CMyComPtr<IArchiveExtractCallbackMessage2> callbackMessage;
    m_ExtractCallback.QueryInterface(IID_IArchiveExtractCallbackMessage2, &callbackMessage);
    if (callbackMessage)
    {
      RINOK(callbackMessage->ReportExtractResult(NEventIndexType::kBlockIndex, folderIndex, NExtract::NOperationResult::kDataError))
    }
    return S_OK;
  }

  for (;;)
  {
    if (!NeedMoreWrite())
      return S_OK;
    UInt64 remain = GetRemain();
    UInt32 size = (UInt32)1 << 20;
    if (size > remain)
      size = (UInt32)remain;
    UInt32 processedSizeLocal = 0;
    RINOK(Write(NULL, size, &processedSizeLocal))
  }
}


HRESULT CFolderOutStream::Unsupported()
{
  while (m_CurrentIndex < m_ExtractStatuses->Size())
  {
    HRESULT result = OpenFile();
    if (result != S_FALSE && result != S_OK)
      return result;
    m_RealOutStream.Release();
    RINOK(m_ExtractCallback->SetOperationResult(NExtract::NOperationResult::kUnsupportedMethod))
    m_CurrentIndex++;
  }
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testModeSpec, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = m_Database.Items.Size();
  if (numItems == 0)
    return S_OK;
  bool testMode = (testModeSpec != 0);
  UInt64 totalUnPacked = 0;

  UInt32 i;
  int lastFolder = -2;
  UInt64 lastFolderSize = 0;
  
  for (i = 0; i < numItems; i++)
  {
    unsigned index = allFilesMode ? i : indices[i];
    const CMvItem &mvItem = m_Database.Items[index];
    const CItem &item = m_Database.Volumes[mvItem.VolumeIndex].Items[mvItem.ItemIndex];
    if (item.IsDir())
      continue;
    int folderIndex = m_Database.GetFolderIndex(&mvItem);
    if (folderIndex != lastFolder)
      totalUnPacked += lastFolderSize;
    lastFolder = folderIndex;
    lastFolderSize = item.GetEndOffset();
  }
  
  totalUnPacked += lastFolderSize;

  extractCallback->SetTotal(totalUnPacked);

  totalUnPacked = 0;

  UInt64 totalPacked = 0;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder;
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  NCompress::NDeflate::NDecoder::CCOMCoder *deflateDecoderSpec = NULL;
  CMyComPtr<ICompressCoder> deflateDecoder;

  NCompress::NLzx::CDecoder *lzxDecoderSpec = NULL;
  CMyComPtr<IUnknown> lzxDecoder;

  NCompress::NQuantum::CDecoder *quantumDecoderSpec = NULL;
  CMyComPtr<IUnknown> quantumDecoder;

  CCabBlockInStream *cabBlockInStreamSpec = new CCabBlockInStream();
  CMyComPtr<ISequentialInStream> cabBlockInStream = cabBlockInStreamSpec;
  if (!cabBlockInStreamSpec->Create())
    return E_OUTOFMEMORY;

  CRecordVector<bool> extractStatuses;
  
  for (i = 0;;)
  {
    lps->OutSize = totalUnPacked;
    lps->InSize = totalPacked;
    RINOK(lps->SetCur())

    if (i >= numItems)
      break;

    const unsigned index = allFilesMode ? i : indices[i];

    const CMvItem &mvItem = m_Database.Items[index];
    const CDatabaseEx &db = m_Database.Volumes[mvItem.VolumeIndex];
    const unsigned itemIndex = mvItem.ItemIndex;
    const CItem &item = db.Items[itemIndex];

    i++;
    if (item.IsDir())
    {
      const Int32 askMode = testMode ?
          NExtract::NAskMode::kTest :
          NExtract::NAskMode::kExtract;
      CMyComPtr<ISequentialOutStream> realOutStream;
      RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
      RINOK(extractCallback->PrepareOperation(askMode))
      realOutStream.Release();
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    
    const int folderIndex = m_Database.GetFolderIndex(&mvItem);
    
    if (folderIndex < 0)
    {
      // If we need previous archive
      const Int32 askMode= testMode ?
          NExtract::NAskMode::kTest :
          NExtract::NAskMode::kExtract;
      CMyComPtr<ISequentialOutStream> realOutStream;
      RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
      RINOK(extractCallback->PrepareOperation(askMode))
      realOutStream.Release();
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kDataError))
      continue;
    }
    
    const unsigned startIndex2 = m_Database.FolderStartFileIndex[(unsigned)folderIndex];
    unsigned startIndex = startIndex2;
    extractStatuses.Clear();
    for (; startIndex < index; startIndex++)
      extractStatuses.Add(false);
    extractStatuses.Add(true);
    startIndex++;
    UInt64 curUnpack = item.GetEndOffset();
    
    for (; i < numItems; i++)
    {
      const unsigned indexNext = allFilesMode ? i : indices[i];
      const CMvItem &mvItem2 = m_Database.Items[indexNext];
      const CItem &item2 = m_Database.Volumes[mvItem2.VolumeIndex].Items[mvItem2.ItemIndex];
      if (item2.IsDir())
        continue;
      const int newFolderIndex = m_Database.GetFolderIndex(&mvItem2);

      if (newFolderIndex != folderIndex)
        break;
      for (; startIndex < indexNext; startIndex++)
        extractStatuses.Add(false);
      extractStatuses.Add(true);
      startIndex++;
      curUnpack = item2.GetEndOffset();
    }

    CFolderOutStream *cabFolderOutStream = new CFolderOutStream;
    CMyComPtr<ISequentialOutStream> outStream(cabFolderOutStream);

    const int folderIndex2 = item.GetFolderIndex(db.Folders.Size());
    if (folderIndex2 < 0)
      return E_FAIL;
    const CFolder &folder = db.Folders[(unsigned)folderIndex2];

    cabFolderOutStream->Init(&m_Database, &extractStatuses, startIndex2,
        curUnpack, extractCallback, testMode);

    cabBlockInStreamSpec->MsZip = false;
    HRESULT res = S_OK;
    
    switch (folder.GetMethod())
    {
      case NHeader::NMethod::kNone:
        break;
      
      case NHeader::NMethod::kMSZip:
        if (!deflateDecoder)
        {
          deflateDecoderSpec = new NCompress::NDeflate::NDecoder::CCOMCoder;
          deflateDecoder = deflateDecoderSpec;
        }
        cabBlockInStreamSpec->MsZip = true;
        break;
      
      case NHeader::NMethod::kLZX:
        if (!lzxDecoder)
        {
          lzxDecoderSpec = new NCompress::NLzx::CDecoder;
          lzxDecoder = lzxDecoderSpec;
        }
        res = lzxDecoderSpec->SetParams_and_Alloc(folder.MethodMinor);
        break;

      case NHeader::NMethod::kQuantum:
        if (!quantumDecoder)
        {
          quantumDecoderSpec = new NCompress::NQuantum::CDecoder;
          quantumDecoder = quantumDecoderSpec;
        }
        res = quantumDecoderSpec->SetParams(folder.MethodMinor);
        break;
      
      default:
        res = E_INVALIDARG;
        break;
    }

    if (res == E_INVALIDARG)
    {
      RINOK(cabFolderOutStream->Unsupported())
      totalUnPacked += curUnpack;
      continue;
    }
    RINOK(res)

    {
      unsigned volIndex = mvItem.VolumeIndex;
      int locFolderIndex = item.GetFolderIndex(db.Folders.Size());
      bool keepHistory = false;
      bool keepInputBuffer = false;
      bool thereWasNotAlignedChunk = false;
      
      for (UInt32 bl = 0; cabFolderOutStream->NeedMoreWrite();)
      {
        if (volIndex >= m_Database.Volumes.Size())
        {
          res = S_FALSE;
          break;
        }

        const CDatabaseEx &db2 = m_Database.Volumes[volIndex];
        if (locFolderIndex < 0)
          return E_FAIL;
        const CFolder &folder2 = db2.Folders[(unsigned)locFolderIndex];
        
        if (bl == 0)
        {
          cabBlockInStreamSpec->ReservedSize = db2.ArcInfo.GetDataBlockReserveSize();
          RINOK(InStream_SeekSet(db2.Stream, db2.StartPosition + folder2.DataStart))
        }
        
        if (bl == folder2.NumDataBlocks)
        {
          /*
            CFolder::NumDataBlocks (CFFOLDER::cCFData in CAB specification) is 16-bit.
            But there are some big CAB archives from MS that contain more
            than (0xFFFF) CFDATA blocks in folder.
            Old cab extracting software can show error (or ask next volume)
            but cab extracting library in new Windows ignores this error.
            15.00 : We also try to ignore such error, if archive is not multi-volume.
          */
          if (m_Database.Volumes.Size() > 1)
          {
            volIndex++;
            locFolderIndex = 0;
            bl = 0;
            continue;
          }
        }
        
        bl++;

        if (!keepInputBuffer)
          cabBlockInStreamSpec->InitForNewBlock();

        UInt32 packSize, unpackSize;
        res = cabBlockInStreamSpec->PreRead(db2.Stream, packSize, unpackSize);
        if (res == S_FALSE)
          break;
        RINOK(res)
        keepInputBuffer = (unpackSize == 0);
        if (keepInputBuffer)
          continue;

        UInt64 totalUnPacked2 = totalUnPacked + cabFolderOutStream->GetPosInFolder();
        totalPacked += packSize;

        lps->OutSize = totalUnPacked2;
        lps->InSize = totalPacked;
        RINOK(lps->SetCur())

        const UInt32 kBlockSizeMax = (1 << 15);

        /* We don't try to reduce last block.
           Note that LZX converts data with x86 filter.
           and filter needs larger input data than reduced size.
           It's simpler to decompress full chunk here.
           also we need full block for quantum for more integrity checks */

        if (unpackSize > kBlockSizeMax)
        {
          res = S_FALSE;
          break;
        }

        if (unpackSize != kBlockSizeMax)
        {
          if (thereWasNotAlignedChunk)
          {
            res = S_FALSE;
            break;
          }
          thereWasNotAlignedChunk = true;
        }

        UInt64 unpackSize64 = unpackSize;
        UInt32 packSizeChunk = cabBlockInStreamSpec->GetPackSizeAvail();

        switch (folder2.GetMethod())
        {
          case NHeader::NMethod::kNone:
            res = copyCoder->Code(cabBlockInStream, outStream, NULL, &unpackSize64, NULL);
            break;
          
          case NHeader::NMethod::kMSZip:
            deflateDecoderSpec->Set_KeepHistory(keepHistory);
            /* v9.31: now we follow MSZIP specification that requires to finish deflate stream at the end of each block.
               But PyCabArc can create CAB archives that doesn't have finish marker at the end of block.
               Cabarc probably ignores such errors in cab archives.
               Maybe we also should ignore that error?
               Or we should extract full file and show the warning? */
            deflateDecoderSpec->Set_NeedFinishInput(true);
            res = deflateDecoder->Code(cabBlockInStream, outStream, NULL, &unpackSize64, NULL);
            if (res == S_OK)
            {
              if (!deflateDecoderSpec->IsFinished())
                res = S_FALSE;
              if (!deflateDecoderSpec->IsFinalBlock())
                res = S_FALSE;
            }
            break;

          case NHeader::NMethod::kLZX:
            lzxDecoderSpec->SetKeepHistory(keepHistory);
            lzxDecoderSpec->KeepHistoryForNext = true;
            
            res = lzxDecoderSpec->Code(cabBlockInStreamSpec->GetData(), packSizeChunk, unpackSize);

            if (res == S_OK)
              res = WriteStream(outStream,
                  lzxDecoderSpec->GetUnpackData(),
                  lzxDecoderSpec->GetUnpackSize());
            break;
          
          case NHeader::NMethod::kQuantum:
            res = quantumDecoderSpec->Code(cabBlockInStreamSpec->GetData(),
                packSizeChunk, outStream, unpackSize, keepHistory);
            break;
        }
      
        if (res != S_OK)
        {
          if (res != S_FALSE)
            RINOK(res)
          break;
        }
        
        keepHistory = true;
      }
      
      if (res == S_OK)
      {
        RINOK(cabFolderOutStream->WriteEmptyFiles())
      }
    }

    if (res != S_OK || cabFolderOutStream->NeedMoreWrite())
    {
      RINOK(cabFolderOutStream->FlushCorrupted((unsigned)folderIndex2))
    }

    totalUnPacked += curUnpack;
  }

  return S_OK;

  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = m_Database.Items.Size();
  return S_OK;
}

}}
