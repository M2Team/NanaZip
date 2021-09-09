// ComHandler.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/IntToString.h"
#include "../../Common/ComTry.h"
#include "../../Common/MyCom.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)

namespace NArchive {
namespace NCom {

#define SIGNATURE { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 }
static const Byte kSignature[] = SIGNATURE;

enum EType
{
  k_Type_Common,
  k_Type_Msi,
  k_Type_Msp,
  k_Type_Doc,
  k_Type_Ppt,
  k_Type_Xls
};

static const char * const kExtensions[] =
{
    "compound"
  , "msi"
  , "msp"
  , "doc"
  , "ppt"
  , "xls"
};

namespace NFatID
{
  // static const UInt32 kFree       = 0xFFFFFFFF;
  static const UInt32 kEndOfChain = 0xFFFFFFFE;
  // static const UInt32 kFatSector  = 0xFFFFFFFD;
  // static const UInt32 kMatSector  = 0xFFFFFFFC;
  static const UInt32 kMaxValue   = 0xFFFFFFFA;
}

namespace NItemType
{
  static const Byte kEmpty = 0;
  static const Byte kStorage = 1;
  // static const Byte kStream = 2;
  // static const Byte kLockBytes = 3;
  // static const Byte kProperty = 4;
  static const Byte kRootStorage = 5;
}

static const UInt32 kNameSizeMax = 64;

struct CItem
{
  Byte Name[kNameSizeMax];
  // UInt16 NameSize;
  // UInt32 Flags;
  FILETIME CTime;
  FILETIME MTime;
  UInt64 Size;
  UInt32 LeftDid;
  UInt32 RightDid;
  UInt32 SonDid;
  UInt32 Sid;
  Byte Type;

  bool IsEmpty() const { return Type == NItemType::kEmpty; }
  bool IsDir() const { return Type == NItemType::kStorage || Type == NItemType::kRootStorage; }

  void Parse(const Byte *p, bool mode64bit);
};

struct CRef
{
  int Parent;
  UInt32 Did;
};

class CDatabase
{
  UInt32 NumSectorsInMiniStream;
  CObjArray<UInt32> MiniSids;

  HRESULT AddNode(int parent, UInt32 did);
public:

  CObjArray<UInt32> Fat;
  UInt32 FatSize;
  
  CObjArray<UInt32> Mat;
  UInt32 MatSize;

  CObjectVector<CItem> Items;
  CRecordVector<CRef> Refs;

  UInt32 LongStreamMinSize;
  unsigned SectorSizeBits;
  unsigned MiniSectorSizeBits;

  Int32 MainSubfile;

  UInt64 PhySize;
  EType Type;

  bool IsNotArcType() const
  {
    return
      Type != k_Type_Msi &&
      Type != k_Type_Msp;
  }

  void UpdatePhySize(UInt64 val)
  {
    if (PhySize < val)
      PhySize = val;
  }
  HRESULT ReadSector(IInStream *inStream, Byte *buf, unsigned sectorSizeBits, UInt32 sid);
  HRESULT ReadIDs(IInStream *inStream, Byte *buf, unsigned sectorSizeBits, UInt32 sid, UInt32 *dest);

  HRESULT Update_PhySize_WithItem(unsigned index);

  void Clear();
  bool IsLargeStream(UInt64 size) const { return size >= LongStreamMinSize; }
  UString GetItemPath(UInt32 index) const;

  UInt64 GetItemPackSize(UInt64 size) const
  {
    UInt64 mask = ((UInt64)1 << (IsLargeStream(size) ? SectorSizeBits : MiniSectorSizeBits)) - 1;
    return (size + mask) & ~mask;
  }

  bool GetMiniCluster(UInt32 sid, UInt64 &res) const
  {
    unsigned subBits = SectorSizeBits - MiniSectorSizeBits;
    UInt32 fid = sid >> subBits;
    if (fid >= NumSectorsInMiniStream)
      return false;
    res = (((UInt64)MiniSids[fid] + 1) << subBits) + (sid & ((1 << subBits) - 1));
    return true;
  }

  HRESULT Open(IInStream *inStream);
};


HRESULT CDatabase::ReadSector(IInStream *inStream, Byte *buf, unsigned sectorSizeBits, UInt32 sid)
{
  UpdatePhySize(((UInt64)sid + 2) << sectorSizeBits);
  RINOK(inStream->Seek((((UInt64)sid + 1) << sectorSizeBits), STREAM_SEEK_SET, NULL));
  return ReadStream_FALSE(inStream, buf, (size_t)1 << sectorSizeBits);
}

HRESULT CDatabase::ReadIDs(IInStream *inStream, Byte *buf, unsigned sectorSizeBits, UInt32 sid, UInt32 *dest)
{
  RINOK(ReadSector(inStream, buf, sectorSizeBits, sid));
  UInt32 sectorSize = (UInt32)1 << sectorSizeBits;
  for (UInt32 t = 0; t < sectorSize; t += 4)
    *dest++ = Get32(buf + t);
  return S_OK;
}

static void GetFileTimeFromMem(const Byte *p, FILETIME *ft)
{
  ft->dwLowDateTime = Get32(p);
  ft->dwHighDateTime = Get32(p + 4);
}

void CItem::Parse(const Byte *p, bool mode64bit)
{
  memcpy(Name, p, kNameSizeMax);
  // NameSize = Get16(p + 64);
  Type = p[66];
  LeftDid = Get32(p + 68);
  RightDid = Get32(p + 72);
  SonDid = Get32(p + 76);
  // Flags = Get32(p + 96);
  GetFileTimeFromMem(p + 100, &CTime);
  GetFileTimeFromMem(p + 108, &MTime);
  Sid = Get32(p + 116);
  Size = Get32(p + 120);
  if (mode64bit)
    Size |= ((UInt64)Get32(p + 124) << 32);
}

void CDatabase::Clear()
{
  PhySize = 0;

  Fat.Free();
  MiniSids.Free();
  Mat.Free();
  Items.Clear();
  Refs.Clear();
}

static const UInt32 kNoDid = 0xFFFFFFFF;

HRESULT CDatabase::AddNode(int parent, UInt32 did)
{
  if (did == kNoDid)
    return S_OK;
  if (did >= (UInt32)Items.Size())
    return S_FALSE;
  const CItem &item = Items[did];
  if (item.IsEmpty())
    return S_FALSE;
  CRef ref;
  ref.Parent = parent;
  ref.Did = did;
  int index = Refs.Add(ref);
  if (Refs.Size() > Items.Size())
    return S_FALSE;
  RINOK(AddNode(parent, item.LeftDid));
  RINOK(AddNode(parent, item.RightDid));
  if (item.IsDir())
  {
    RINOK(AddNode(index, item.SonDid));
  }
  return S_OK;
}

static UString CompoundNameToFileName(const UString &s)
{
  UString res;
  for (unsigned i = 0; i < s.Len(); i++)
  {
    wchar_t c = s[i];
    if (c < 0x20)
    {
      res += '[';
      res.Add_UInt32(c);
      res += ']';
    }
    else
      res += c;
  }
  return res;
}

static const char k_Msi_Chars[] =
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz._";

// static const char * const k_Msi_ID = ""; // "{msi}";
static const char k_Msi_SpecChar = '!';

static const unsigned k_Msi_NumBits = 6;
static const unsigned k_Msi_NumChars = 1 << k_Msi_NumBits;
static const unsigned k_Msi_CharMask = k_Msi_NumChars - 1;
static const unsigned k_Msi_StartUnicodeChar = 0x3800;
static const unsigned k_Msi_UnicodeRange = k_Msi_NumChars * (k_Msi_NumChars + 1);


static bool IsMsiName(const Byte *p)
{
  UInt32 c = Get16(p);
  return
      c >= k_Msi_StartUnicodeChar &&
      c <= k_Msi_StartUnicodeChar + k_Msi_UnicodeRange;
}

static bool AreEqualNames(const Byte *rawName, const char *asciiName)
{
  for (unsigned i = 0; i < kNameSizeMax / 2; i++)
  {
    wchar_t c = Get16(rawName + i * 2);
    wchar_t c2 = (Byte)asciiName[i];
    if (c != c2)
      return false;
    if (c == 0)
      return true;
  }
  return false;
}

static bool CompoundMsiNameToFileName(const UString &name, UString &res)
{
  res.Empty();
  for (unsigned i = 0; i < name.Len(); i++)
  {
    wchar_t c = name[i];
    if (c < (wchar_t)k_Msi_StartUnicodeChar || c > (wchar_t)(k_Msi_StartUnicodeChar + k_Msi_UnicodeRange))
      return false;
    /*
    if (i == 0)
      res += k_Msi_ID;
    */
    c -= k_Msi_StartUnicodeChar;
    
    unsigned c0 = (unsigned)c & k_Msi_CharMask;
    unsigned c1 = (unsigned)c >> k_Msi_NumBits;

    if (c1 <= k_Msi_NumChars)
    {
      res += k_Msi_Chars[c0];
      if (c1 == k_Msi_NumChars)
        break;
      res += k_Msi_Chars[c1];
    }
    else
      res += k_Msi_SpecChar;
  }
  return true;
}

static UString ConvertName(const Byte *p, bool &isMsi)
{
  isMsi = false;
  UString s;
  
  for (unsigned i = 0; i < kNameSizeMax; i += 2)
  {
    wchar_t c = Get16(p + i);
    if (c == 0)
      break;
    s += c;
  }
  
  UString msiName;
  if (CompoundMsiNameToFileName(s, msiName))
  {
    isMsi = true;
    return msiName;
  }
  return CompoundNameToFileName(s);
}

static UString ConvertName(const Byte *p)
{
  bool isMsi;
  return ConvertName(p, isMsi);
}

UString CDatabase::GetItemPath(UInt32 index) const
{
  UString s;
  while (index != kNoDid)
  {
    const CRef &ref = Refs[index];
    const CItem &item = Items[ref.Did];
    if (!s.IsEmpty())
      s.InsertAtFront(WCHAR_PATH_SEPARATOR);
    s.Insert(0, ConvertName(item.Name));
    index = ref.Parent;
  }
  return s;
}

HRESULT CDatabase::Update_PhySize_WithItem(unsigned index)
{
  const CItem &item = Items[index];
  bool isLargeStream = (index == 0 || IsLargeStream(item.Size));
  if (!isLargeStream)
    return S_OK;
  unsigned bsLog = isLargeStream ? SectorSizeBits : MiniSectorSizeBits;
  // streamSpec->Size = item.Size;
  
  UInt32 clusterSize = (UInt32)1 << bsLog;
  UInt64 numClusters64 = (item.Size + clusterSize - 1) >> bsLog;
  if (numClusters64 >= ((UInt32)1 << 31))
    return S_FALSE;
  UInt32 sid = item.Sid;
  UInt64 size = item.Size;
  
  if (size != 0)
  {
    for (;; size -= clusterSize)
    {
      // if (isLargeStream)
      {
        if (sid >= FatSize)
          return S_FALSE;
        UpdatePhySize(((UInt64)sid + 2) << bsLog);
        sid = Fat[sid];
      }
      if (size <= clusterSize)
        break;
    }
  }
  if (sid != NFatID::kEndOfChain)
    return S_FALSE;
  return S_OK;
}

// There is name "[!]MsiPatchSequence" in msp files
static const unsigned kMspSequence_Size = 18;
static const Byte kMspSequence[kMspSequence_Size] =
  { 0x40, 0x48, 0x96, 0x45, 0x6C, 0x3E, 0xE4, 0x45,
    0xE6, 0x42, 0x16, 0x42, 0x37, 0x41, 0x27, 0x41,
    0x37, 0x41 };

HRESULT CDatabase::Open(IInStream *inStream)
{
  MainSubfile = -1;
  Type = k_Type_Common;
  const UInt32 kHeaderSize = 512;
  Byte p[kHeaderSize];
  PhySize = kHeaderSize;
  RINOK(ReadStream_FALSE(inStream, p, kHeaderSize));
  if (memcmp(p, kSignature, ARRAY_SIZE(kSignature)) != 0)
    return S_FALSE;
  if (Get16(p + 0x1A) > 4) // majorVer
    return S_FALSE;
  if (Get16(p + 0x1C) != 0xFFFE) // Little-endian
    return S_FALSE;
  unsigned sectorSizeBits = Get16(p + 0x1E);
  bool mode64bit = (sectorSizeBits >= 12);
  unsigned miniSectorSizeBits = Get16(p + 0x20);
  SectorSizeBits = sectorSizeBits;
  MiniSectorSizeBits = miniSectorSizeBits;

  if (sectorSizeBits > 24 ||
      sectorSizeBits < 7 ||
      miniSectorSizeBits > 24 ||
      miniSectorSizeBits < 2 ||
      miniSectorSizeBits > sectorSizeBits)
    return S_FALSE;
  UInt32 numSectorsForFAT = Get32(p + 0x2C); // SAT
  LongStreamMinSize = Get32(p + 0x38);
  
  UInt32 sectSize = (UInt32)1 << sectorSizeBits;

  CByteBuffer sect(sectSize);

  unsigned ssb2 = sectorSizeBits - 2;
  UInt32 numSidsInSec = (UInt32)1 << ssb2;
  UInt32 numFatItems = numSectorsForFAT << ssb2;
  if ((numFatItems >> ssb2) != numSectorsForFAT)
    return S_FALSE;
  FatSize = numFatItems;

  {
    UInt32 numSectorsForBat = Get32(p + 0x48); // master sector allocation table
    const UInt32 kNumHeaderBatItems = 109;
    UInt32 numBatItems = kNumHeaderBatItems + (numSectorsForBat << ssb2);
    if (numBatItems < kNumHeaderBatItems || ((numBatItems - kNumHeaderBatItems) >> ssb2) != numSectorsForBat)
      return S_FALSE;
    CObjArray<UInt32> bat(numBatItems);
    UInt32 i;
    for (i = 0; i < kNumHeaderBatItems; i++)
      bat[i] = Get32(p + 0x4c + i * 4);
    UInt32 sid = Get32(p + 0x44);
    for (UInt32 s = 0; s < numSectorsForBat; s++)
    {
      RINOK(ReadIDs(inStream, sect, sectorSizeBits, sid, bat + i));
      i += numSidsInSec - 1;
      sid = bat[i];
    }
    numBatItems = i;
    
    Fat.Alloc(numFatItems);
    UInt32 j = 0;
      
    for (i = 0; i < numFatItems; j++, i += numSidsInSec)
    {
      if (j >= numBatItems)
        return S_FALSE;
      RINOK(ReadIDs(inStream, sect, sectorSizeBits, bat[j], Fat + i));
    }
    FatSize = numFatItems = i;
  }

  UInt32 numMatItems;
  {
    UInt32 numSectorsForMat = Get32(p + 0x40);
    numMatItems = (UInt32)numSectorsForMat << ssb2;
    if ((numMatItems >> ssb2) != numSectorsForMat)
      return S_FALSE;
    Mat.Alloc(numMatItems);
    UInt32 i;
    UInt32 sid = Get32(p + 0x3C); // short-sector table SID
    for (i = 0; i < numMatItems; i += numSidsInSec)
    {
      RINOK(ReadIDs(inStream, sect, sectorSizeBits, sid, Mat + i));
      if (sid >= numFatItems)
        return S_FALSE;
      sid = Fat[sid];
    }
    if (sid != NFatID::kEndOfChain)
      return S_FALSE;
  }

  {
    CByteBuffer used(numFatItems);
    for (UInt32 i = 0; i < numFatItems; i++)
      used[i] = 0;
    UInt32 sid = Get32(p + 0x30); // directory stream SID
    for (;;)
    {
      if (sid >= numFatItems)
        return S_FALSE;
      if (used[sid])
        return S_FALSE;
      used[sid] = 1;
      RINOK(ReadSector(inStream, sect, sectorSizeBits, sid));
      for (UInt32 i = 0; i < sectSize; i += 128)
      {
        CItem item;
        item.Parse(sect + i, mode64bit);
        Items.Add(item);
      }
      sid = Fat[sid];
      if (sid == NFatID::kEndOfChain)
        break;
    }
  }

  const CItem &root = Items[0];

  {
    UInt32 numSectorsInMiniStream;
    {
      UInt64 numSatSects64 = (root.Size + sectSize - 1) >> sectorSizeBits;
      if (numSatSects64 > NFatID::kMaxValue)
        return S_FALSE;
      numSectorsInMiniStream = (UInt32)numSatSects64;
    }
    NumSectorsInMiniStream = numSectorsInMiniStream;
    MiniSids.Alloc(numSectorsInMiniStream);
    {
      UInt64 matSize64 = (root.Size + ((UInt64)1 << miniSectorSizeBits) - 1) >> miniSectorSizeBits;
      if (matSize64 > NFatID::kMaxValue)
        return S_FALSE;
      MatSize = (UInt32)matSize64;
      if (numMatItems < MatSize)
        return S_FALSE;
    }

    UInt32 sid = root.Sid;
    for (UInt32 i = 0; ; i++)
    {
      if (sid == NFatID::kEndOfChain)
      {
        if (i != numSectorsInMiniStream)
          return S_FALSE;
        break;
      }
      if (i >= numSectorsInMiniStream)
        return S_FALSE;
      MiniSids[i] = sid;
      if (sid >= numFatItems)
        return S_FALSE;
      sid = Fat[sid];
    }
  }

  RINOK(AddNode(-1, root.SonDid));
  
  unsigned numCabs = 0;
  
  FOR_VECTOR (i, Refs)
  {
    const CItem &item = Items[Refs[i].Did];
    if (item.IsDir() || numCabs > 1)
      continue;
    bool isMsiName;
    const UString msiName = ConvertName(item.Name, isMsiName);
    if (isMsiName && !msiName.IsEmpty())
    {
      // bool isThereExt = (msiName.Find(L'.') >= 0);
      bool isMsiSpec = (msiName[0] == k_Msi_SpecChar);
      if ((msiName.Len() >= 4 && StringsAreEqualNoCase_Ascii(msiName.RightPtr(4), ".cab"))
          || (!isMsiSpec && msiName.Len() >= 3 && StringsAreEqualNoCase_Ascii(msiName.RightPtr(3), "exe"))
          // || (!isMsiSpec && !isThereExt)
          )
      {
        numCabs++;
        MainSubfile = i;
      }
    }
  }
  
  if (numCabs > 1)
    MainSubfile = -1;

  {
    FOR_VECTOR (t, Items)
    {
      Update_PhySize_WithItem(t);
    }
  }
  {
    FOR_VECTOR (t, Items)
    {
      const CItem &item = Items[t];

      if (IsMsiName(item.Name))
      {
        Type = k_Type_Msi;
        if (memcmp(item.Name, kMspSequence, kMspSequence_Size) == 0)
        {
          Type = k_Type_Msp;
          break;
        }
        continue;
      }
      if (AreEqualNames(item.Name, "WordDocument"))
      {
        Type = k_Type_Doc;
        break;
      }
      if (AreEqualNames(item.Name, "PowerPoint Document"))
      {
        Type = k_Type_Ppt;
        break;
      }
      if (AreEqualNames(item.Name, "Workbook"))
      {
        Type = k_Type_Xls;
        break;
      }
    }
  }

  return S_OK;
}

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _stream;
  CDatabase _db;
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidCTime,
  kpidMTime
};

static const Byte kArcProps[] =
{
  kpidExtension,
  kpidClusterSize,
  kpidSectorSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidExtension: prop = kExtensions[(unsigned)_db.Type]; break;
    case kpidPhySize: prop = _db.PhySize; break;
    case kpidClusterSize: prop = (UInt32)1 << _db.SectorSizeBits; break;
    case kpidSectorSize: prop = (UInt32)1 << _db.MiniSectorSizeBits; break;
    case kpidMainSubfile: if (_db.MainSubfile >= 0) prop = (UInt32)_db.MainSubfile; break;
    case kpidIsNotArcType: if (_db.IsNotArcType()) prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CRef &ref = _db.Refs[index];
  const CItem &item = _db.Items[ref.Did];
    
  switch (propID)
  {
    case kpidPath:  prop = _db.GetItemPath(index); break;
    case kpidIsDir:  prop = item.IsDir(); break;
    case kpidCTime:  prop = item.CTime; break;
    case kpidMTime:  prop = item.MTime; break;
    case kpidPackSize:  if (!item.IsDir()) prop = _db.GetItemPackSize(item.Size); break;
    case kpidSize:  if (!item.IsDir()) prop = item.Size; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */)
{
  COM_TRY_BEGIN
  Close();
  try
  {
    if (_db.Open(inStream) != S_OK)
      return S_FALSE;
    _stream = inStream;
  }
  catch(...) { return S_FALSE; }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _db.Clear();
  _stream.Release();
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _db.Refs.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  UInt64 totalSize = 0;
  for (i = 0; i < numItems; i++)
  {
    const CItem &item = _db.Items[_db.Refs[allFilesMode ? i : indices[i]].Did];
    if (!item.IsDir())
      totalSize += item.Size;
  }
  RINOK(extractCallback->SetTotal(totalSize));

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur());
    Int32 index = allFilesMode ? i : indices[i];
    const CItem &item = _db.Items[_db.Refs[index].Did];

    CMyComPtr<ISequentialOutStream> outStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(index, &outStream, askMode));

    if (item.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode));
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }

    totalPackSize += _db.GetItemPackSize(item.Size);
    totalSize += item.Size;
    
    if (!testMode && !outStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));
    Int32 res = NExtract::NOperationResult::kDataError;
    CMyComPtr<ISequentialInStream> inStream;
    HRESULT hres = GetStream(index, &inStream);
    if (hres == S_FALSE)
      res = NExtract::NOperationResult::kDataError;
    else if (hres == E_NOTIMPL)
      res = NExtract::NOperationResult::kUnsupportedMethod;
    else
    {
      RINOK(hres);
      if (inStream)
      {
        RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress));
        if (copyCoderSpec->TotalSize == item.Size)
          res = NExtract::NOperationResult::kOK;
      }
    }
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(res));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _db.Refs.Size();
  return S_OK;
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  *stream = 0;
  UInt32 itemIndex = _db.Refs[index].Did;
  const CItem &item = _db.Items[itemIndex];
  CClusterInStream *streamSpec = new CClusterInStream;
  CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
  streamSpec->Stream = _stream;
  streamSpec->StartOffset = 0;

  bool isLargeStream = (itemIndex == 0 || _db.IsLargeStream(item.Size));
  int bsLog = isLargeStream ? _db.SectorSizeBits : _db.MiniSectorSizeBits;
  streamSpec->BlockSizeLog = bsLog;
  streamSpec->Size = item.Size;

  UInt32 clusterSize = (UInt32)1 << bsLog;
  UInt64 numClusters64 = (item.Size + clusterSize - 1) >> bsLog;
  if (numClusters64 >= ((UInt32)1 << 31))
    return E_NOTIMPL;
  streamSpec->Vector.ClearAndReserve((unsigned)numClusters64);
  UInt32 sid = item.Sid;
  UInt64 size = item.Size;

  if (size != 0)
  {
    for (;; size -= clusterSize)
    {
      if (isLargeStream)
      {
        if (sid >= _db.FatSize)
          return S_FALSE;
        streamSpec->Vector.AddInReserved(sid + 1);
        sid = _db.Fat[sid];
      }
      else
      {
        UInt64 val = 0;
        if (sid >= _db.MatSize || !_db.GetMiniCluster(sid, val) || val >= (UInt64)1 << 32)
          return S_FALSE;
        streamSpec->Vector.AddInReserved((UInt32)val);
        sid = _db.Mat[sid];
      }
      if (size <= clusterSize)
        break;
    }
  }
  if (sid != NFatID::kEndOfChain)
    return S_FALSE;
  RINOK(streamSpec->InitAndSeek());
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I(
  "Compound", "msi msp doc xls ppt", 0, 0xE5,
  kSignature,
  0,
  0,
  NULL)

}}
