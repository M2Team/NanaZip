// TarIn.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/StringToInt.h"

#include "../../Common/StreamUtils.h"

#include "../IArchive.h"

#include "TarIn.h"

#define NUM_UNROLL_BYTES (8 * 4)

Z7_NO_INLINE static bool IsBufNonZero(const void *data, size_t size);
Z7_NO_INLINE static bool IsBufNonZero(const void *data, size_t size)
{
  const Byte *p = (const Byte *)data;

  for (; size != 0 && ((unsigned)(ptrdiff_t)p & (NUM_UNROLL_BYTES - 1)) != 0; size--)
    if (*p++ != 0)
      return true;

  if (size >= NUM_UNROLL_BYTES)
  {
    const Byte *lim = p + size;
    size &= (NUM_UNROLL_BYTES - 1);
    lim -= size;
    do
    {
      if (*(const UInt64 *)(const void *)(p        ) != 0) return true;
      if (*(const UInt64 *)(const void *)(p + 8 * 1) != 0) return true;
      if (*(const UInt64 *)(const void *)(p + 8 * 2) != 0) return true;
      if (*(const UInt64 *)(const void *)(p + 8 * 3) != 0) return true;
      // if (*(const UInt32 *)(const void *)(p        ) != 0) return true;
      // if (*(const UInt32 *)(const void *)(p + 4 * 1) != 0) return true;
      // if (*(const UInt32 *)(const void *)(p + 4 * 2) != 0) return true;
      // if (*(const UInt32 *)(const void *)(p + 4 * 3) != 0) return true;
      p += NUM_UNROLL_BYTES;
    }
    while (p != lim);
  }
  
  for (; size != 0; size--)
    if (*p++ != 0)
      return true;

  return false;
}


namespace NArchive {
namespace NTar {
 
static void MyStrNCpy(char *dest, const char *src, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
  {
    char c = src[i];
    dest[i] = c;
    if (c == 0)
      break;
  }
}

static bool OctalToNumber(const char *srcString, unsigned size, UInt64 &res, bool allowEmpty = false)
{
  res = 0;
  char sz[32];
  MyStrNCpy(sz, srcString, size);
  sz[size] = 0;
  const char *end;
  unsigned i;
  for (i = 0; sz[i] == ' '; i++);
  if (sz[i] == 0)
    return allowEmpty;
  res = ConvertOctStringToUInt64(sz + i, &end);
  return (*end == ' ' || *end == 0);
}

static bool OctalToNumber32(const char *srcString, UInt32 &res, bool allowEmpty = false)
{
  const unsigned kSize = 8;
  UInt64 res64;
  if (!OctalToNumber(srcString, kSize, res64, allowEmpty))
    return false;
  res = (UInt32)res64;
  return (res64 <= 0xFFFFFFFF);
}

#define RIF(x) { if (!(x)) return S_OK; }

static void ReadString(const char *s, unsigned size, AString &result)
{
  result.SetFrom_CalcLen(s, size);
}

static bool ParseInt64(const char *p, Int64 &val, bool &isBin)
{
  const UInt32 h = GetBe32(p);
  val = (Int64)GetBe64(p + 4);
  isBin = true;
  if (h == (UInt32)1 << 31)
    return ((val >> 63) & 1) == 0;
  if (h == (UInt32)(Int32)-1)
    return ((val >> 63) & 1) != 0;
  isBin = false;
  UInt64 u;
  const bool res = OctalToNumber(p, 12, u);
  val = (Int64)u;
  return res;
}

static bool ParseInt64_MTime(const char *p, Int64 &val, bool &isBin)
{
  // rare case tar : ZEROs in Docker-Windows TARs
  // rare case tar : spaces
  isBin = false;
  if (GetUi32(p) != 0)
  for (unsigned i = 0; i < 12; i++)
    if (p[i] != ' ')
      return ParseInt64(p, val, isBin);
  val = 0;
  return true;
}

static bool ParseSize(const char *p, UInt64 &val, bool &isBin)
{
  if (GetBe32(p) == (UInt32)1 << 31)
  {
    // GNU extension
    isBin = true;
    val = GetBe64(p + 4);
    return ((val >> 63) & 1) == 0;
  }
  isBin = false;
  return OctalToNumber(p, 12, val,
      true // 20.03: allow empty size for 'V' Label entry
      );
}

static bool ParseSize(const char *p, UInt64 &val)
{
  bool isBin;
  return ParseSize(p, val, isBin);
}

#define CHECK(x) { if (!(x)) return k_IsArc_Res_NO; }

API_FUNC_IsArc IsArc_Tar(const Byte *p2, size_t size)
{
  if (size < NFileHeader::kRecordSize)
    return k_IsArc_Res_NEED_MORE;

  const char *p = (const char *)p2;
  p += NFileHeader::kNameSize;

  UInt32 mode;
  // we allow empty Mode value for LongName prefix items
  CHECK(OctalToNumber32(p, mode, true)) p += 8;

  // if (!OctalToNumber32(p, item.UID)) item.UID = 0;
  p += 8;
  // if (!OctalToNumber32(p, item.GID)) item.GID = 0;
  p += 8;

  UInt64 packSize;
  Int64 time;
  UInt32 checkSum;
  bool isBin;
  CHECK(ParseSize(p, packSize, isBin)) p += 12;
  CHECK(ParseInt64_MTime(p, time, isBin)) p += 12;
  CHECK(OctalToNumber32(p, checkSum))
  return k_IsArc_Res_YES;
}


HRESULT CArchive::GetNextItemReal(CItemEx &item)
{
  char buf[NFileHeader::kRecordSize];

  error = k_ErrorType_OK;
  filled = false;

  bool thereAreEmptyRecords = false;
  for (;;)
  {
    size_t processedSize = NFileHeader::kRecordSize;
    RINOK(ReadStream(SeqStream, buf, &processedSize))
    if (processedSize == 0)
    {
      if (!thereAreEmptyRecords)
        error = k_ErrorType_UnexpectedEnd; // "There are no trailing zero-filled records";
      return S_OK;
    }
    if (processedSize != NFileHeader::kRecordSize)
    {
      if (!thereAreEmptyRecords)
        error = k_ErrorType_UnexpectedEnd; // error = "There is no correct record at the end of archive";
      else
      {
        /*
        if (IsEmptyData(buf, processedSize))
          error = k_ErrorType_UnexpectedEnd;
        else
        {
          // extraReadSize = processedSize;
          // error = k_ErrorType_Corrupted; // some data after the end tail zeros
        }
        */
      }

      return S_OK;
    }
    if (IsBufNonZero(buf, NFileHeader::kRecordSize))
      break;
    item.HeaderSize += NFileHeader::kRecordSize;
    thereAreEmptyRecords = true;
    if (OpenCallback)
    {
      RINOK(Progress(item, 0))
    }
  }
  if (thereAreEmptyRecords)
  {
    // error = "There are data after end of archive";
    return S_OK;
  }
  
  char *p = buf;

  error = k_ErrorType_Corrupted;

  // ReadString(p, NFileHeader::kNameSize, item.Name);
  p += NFileHeader::kNameSize;

  /*
  item.Name_CouldBeReduced =
      (item.Name.Len() == NFileHeader::kNameSize ||
       item.Name.Len() == NFileHeader::kNameSize - 1);
  */

  // we allow empty Mode value for LongName prefix items
  RIF(OctalToNumber32(p, item.Mode, true)) p += 8;

  if (!OctalToNumber32(p, item.UID)) { item.UID = 0; }  p += 8;
  if (!OctalToNumber32(p, item.GID)) { item.GID = 0; }  p += 8;

  RIF(ParseSize(p, item.PackSize, item.PackSize_IsBin))
  item.Size = item.PackSize;
  item.Size_IsBin = item.PackSize_IsBin;
  p += 12;
  RIF(ParseInt64_MTime(p, item.MTime, item.MTime_IsBin)) p += 12;
  
  UInt32 checkSum;
  RIF(OctalToNumber32(p, checkSum))
  memset(p, ' ', 8); p += 8;

  item.LinkFlag = *p++;

  ReadString(p, NFileHeader::kNameSize, item.LinkName); p += NFileHeader::kNameSize;
  
  /*
  item.LinkName_CouldBeReduced =
      (item.LinkName.Len() == NFileHeader::kNameSize ||
       item.LinkName.Len() == NFileHeader::kNameSize - 1);
  */

  memcpy(item.Magic, p, 8); p += 8;

  ReadString(p, NFileHeader::kUserNameSize, item.User); p += NFileHeader::kUserNameSize;
  ReadString(p, NFileHeader::kGroupNameSize, item.Group); p += NFileHeader::kGroupNameSize;

  item.DeviceMajor_Defined = (p[0] != 0); if (item.DeviceMajor_Defined) { RIF(OctalToNumber32(p, item.DeviceMajor)) } p += 8;
  item.DeviceMinor_Defined = (p[0] != 0); if (item.DeviceMinor_Defined) { RIF(OctalToNumber32(p, item.DeviceMinor)) } p += 8;

  if (p[0] != 0
      && item.IsMagic_ustar_5chars()
      && (item.LinkFlag != 'L' ))
  {
    item.Prefix_WasUsed = true;
    ReadString(p, NFileHeader::kPrefixSize, item.Name);
    item.Name += '/';
    unsigned i;
    for (i = 0; i < NFileHeader::kNameSize; i++)
      if (buf[i] == 0)
        break;
    item.Name.AddFrom(buf, i);
  }
  else
    ReadString(buf, NFileHeader::kNameSize, item.Name);
  
  p += NFileHeader::kPrefixSize;

  if (item.LinkFlag == NFileHeader::NLinkFlag::kHardLink)
  {
    item.PackSize = 0;
    item.Size = 0;
  }

  if (item.LinkFlag == NFileHeader::NLinkFlag::kDirectory)
  {
    // GNU tar ignores Size field, if LinkFlag is kDirectory
    // 21.02 : we set PackSize = 0 to be more compatible with GNU tar
    item.PackSize = 0;
    // item.Size = 0;
  }

  /*
    TAR standard requires sum of unsigned byte values.
    But some old TAR programs use sum of signed byte values.
    So we check both values.
  */
  // for (int y = 0; y < 100; y++) // for debug
  {
    UInt32 sum0 = 0;
    {
      for (unsigned i = 0; i < NFileHeader::kRecordSize; i++)
        sum0 += (Byte)buf[i];
    }
    if (sum0 != checkSum)
    {
      Int32 sum = 0;
      for (unsigned i = 0; i < NFileHeader::kRecordSize; i++)
        sum += (signed char)buf[i];
      if ((UInt32)sum != checkSum)
        return S_OK;
      item.IsSignedChecksum = true;
    }
  }

  item.HeaderSize += NFileHeader::kRecordSize;

  if (item.LinkFlag == NFileHeader::NLinkFlag::kSparse)
  {
    Byte isExtended = (Byte)buf[482];
    if (isExtended != 0 && isExtended != 1)
      return S_OK;
    RIF(ParseSize(buf + 483, item.Size, item.Size_IsBin))
    UInt64 min = 0;
    for (unsigned i = 0; i < 4; i++)
    {
      p = buf + 386 + 24 * i;
      if (GetBe32(p) == 0)
      {
        if (isExtended != 0)
          return S_OK;
        break;
      }
      CSparseBlock sb;
      RIF(ParseSize(p, sb.Offset))
      RIF(ParseSize(p + 12, sb.Size))
      item.SparseBlocks.Add(sb);
      if (sb.Offset < min || sb.Offset > item.Size)
        return S_OK;
      if ((sb.Offset & 0x1FF) != 0 || (sb.Size & 0x1FF) != 0)
        return S_OK;
      min = sb.Offset + sb.Size;
      if (min < sb.Offset)
        return S_OK;
    }
    if (min > item.Size)
      return S_OK;

    while (isExtended != 0)
    {
      size_t processedSize = NFileHeader::kRecordSize;
      RINOK(ReadStream(SeqStream, buf, &processedSize))
      if (processedSize != NFileHeader::kRecordSize)
      {
        error = k_ErrorType_UnexpectedEnd;
        return S_OK;
      }

      item.HeaderSize += NFileHeader::kRecordSize;

      if (OpenCallback)
      {
        RINOK(Progress(item, 0))
      }

      isExtended = (Byte)buf[21 * 24];
      if (isExtended != 0 && isExtended != 1)
        return S_OK;
      for (unsigned i = 0; i < 21; i++)
      {
        p = buf + 24 * i;
        if (GetBe32(p) == 0)
        {
          if (isExtended != 0)
            return S_OK;
          break;
        }
        CSparseBlock sb;
        RIF(ParseSize(p, sb.Offset))
        RIF(ParseSize(p + 12, sb.Size))
        item.SparseBlocks.Add(sb);
        if (sb.Offset < min || sb.Offset > item.Size)
          return S_OK;
        if ((sb.Offset & 0x1FF) != 0 || (sb.Size & 0x1FF) != 0)
          return S_OK;
        min = sb.Offset + sb.Size;
        if (min < sb.Offset)
          return S_OK;
      }
    }
    if (min > item.Size)
      return S_OK;
  }
 
  if (item.PackSize >= (UInt64)1 << 63)
    return S_OK;

  filled = true;
  error = k_ErrorType_OK;
  return S_OK;
}


HRESULT CArchive::Progress(const CItemEx &item, UInt64 posOffset)
{
  const UInt64 pos = item.Get_DataPos() + posOffset;
  if (NumFiles - NumFiles_Prev < (1 << 16)
      // && NumRecords - NumRecords_Prev < (1 << 16)
      && pos - Pos_Prev < ((UInt32)1 << 28))
    return S_OK;
  {
    Pos_Prev = pos;
    NumFiles_Prev = NumFiles;
    // NumRecords_Prev = NumRecords;
    // Sleep(100); // for debug
    return OpenCallback->SetCompleted(&NumFiles, &pos);
  }
}


HRESULT CArchive::ReadDataToBuffer(const CItemEx &item,
    CTempBuffer &tb, size_t stringLimit)
{
  tb.Init();
  UInt64 packSize = item.Get_PackSize_Aligned();
  if (packSize == 0)
    return S_OK;

  UInt64 pos;

  {
    size_t size = stringLimit;
    if (size > packSize)
      size = (size_t)packSize;
    tb.Buffer.AllocAtLeast(size);
    size_t processedSize = size;
    const HRESULT res = ReadStream(SeqStream, tb.Buffer, &processedSize);
    pos = processedSize;
    if (processedSize != size)
    {
      error = k_ErrorType_UnexpectedEnd;
      return res;
    }
    RINOK(res)

    packSize -= size;
   
    size_t i;
    const Byte *p = tb.Buffer;
    for (i = 0; i < size; i++)
      if (p[i] == 0)
        break;
    if (i >= item.PackSize)
      tb.StringSize_IsConfirmed = true;
    if (i > item.PackSize)
    {
      tb.StringSize = (size_t)item.PackSize;
      tb.IsNonZeroTail = true;
    }
    else
    {
      tb.StringSize = i;
      if (i != size)
      {
        tb.StringSize_IsConfirmed = true;
        if (IsBufNonZero(p + i, size - i))
          tb.IsNonZeroTail = true;
      }
    }
   
    if (packSize == 0)
      return S_OK;
  }

  if (InStream)
  {
    RINOK(InStream->Seek((Int64)packSize, STREAM_SEEK_CUR, NULL))
    return S_OK;
  }
  const unsigned kBufSize = 1 << 15;
  Buffer.AllocAtLeast(kBufSize);

  do
  {
    if (OpenCallback)
    {
      RINOK(Progress(item, pos))
    }

    unsigned size = kBufSize;
    if (size > packSize)
      size = (unsigned)packSize;
    size_t processedSize = size;
    const HRESULT res = ReadStream(SeqStream, Buffer, &processedSize);
    if (processedSize != size)
    {
      error = k_ErrorType_UnexpectedEnd;
      return res;
    }
    if (!tb.IsNonZeroTail)
    {
      if (IsBufNonZero(Buffer, size))
        tb.IsNonZeroTail = true;
    }
    packSize -= size;
    pos += size;
  }
  while (packSize != 0);
  return S_OK;
}
 


struct CPaxInfo: public CPaxTimes
{
  bool DoubleTagError;
  bool TagParsingError;
  bool UnknownLines_Overflow;
  bool Size_Defined;
  bool UID_Defined;
  bool GID_Defined;
  bool Path_Defined;
  bool Link_Defined;
  bool User_Defined;
  bool Group_Defined;
  
  UInt64 Size;
  UInt32 UID;
  UInt32 GID;

  AString Path;
  AString Link;
  AString User;
  AString Group;
  AString UnknownLines;
  
  bool ParseID(const AString &val, bool &defined, UInt32 &res)
  {
    if (defined)
      DoubleTagError = true;
    if (val.IsEmpty())
      return false;
    const char *end2;
    res = ConvertStringToUInt32(val.Ptr(), &end2);
    if (*end2 != 0)
      return false;
    defined = true;
    return true;
  }

  bool ParsePax(const CTempBuffer &tb, bool isFile);
};


static bool ParsePaxTime(const AString &src, CPaxTime &pt, bool &doubleTagError)
{
  if (pt.IsDefined())
    doubleTagError = true;
  pt.Clear();
  const char *s = src.Ptr();
  bool isNegative = false;
  if (*s == '-')
  {
    isNegative = true;
    s++;
  }
  const char *end;
  {
    UInt64 sec = ConvertStringToUInt64(s, &end);
    if (s == end)
      return false;
    if (sec >= ((UInt64)1 << 63))
      return false;
    if (isNegative)
      sec = (UInt64)-(Int64)sec;
    pt.Sec = (Int64)sec;
  }
  if (*end == 0)
  {
    pt.Ns = 0;
    pt.NumDigits = 0;
    return true;
  }
  if (*end != '.')
    return false;
  s = end + 1;

  UInt32 ns = 0;
  unsigned i;
  const unsigned kNsDigits = 9;
  for (i = 0;; i++)
  {
    const char c = s[i];
    if (c == 0)
      break;
    if (c < '0' || c > '9')
      return false;
    // we ignore digits after 9 digits as GNU TAR
    if (i < kNsDigits)
    {
      ns *= 10;
      ns += (unsigned)(c - '0');
    }
  }
  pt.NumDigits = (int)(i < kNsDigits ? i : kNsDigits);
  while (i < kNsDigits)
  {
    ns *= 10;
    i++;
  }
  if (isNegative && ns != 0)
  {
    pt.Sec--;
    ns = (UInt32)1000 * 1000 * 1000 - ns;
  }
  pt.Ns = ns;
  return true;
}


bool CPaxInfo::ParsePax(const CTempBuffer &tb, bool isFile)
{
  DoubleTagError = false;
  TagParsingError = false;
  UnknownLines_Overflow = false;
  Size_Defined = false;
  UID_Defined = false;
  GID_Defined = false;
  Path_Defined = false;
  Link_Defined = false;
  User_Defined = false;
  Group_Defined = false;
  
  // CPaxTimes::Clear();

  const char *s = (const char *)(const void *)(const Byte *)tb.Buffer;
  size_t rem = tb.StringSize;

  Clear();

  AString name, val;

  while (rem != 0)
  {
    unsigned i;
    for (i = 0;; i++)
    {
      if (i > 24 || i >= rem) // we use limitation for size of (size) field
        return false;
      if (s[i] == ' ')
        break;
    }
    if (i == 0)
      return false;
    const char *end;
    const UInt32 size = ConvertStringToUInt32(s, &end);
    const unsigned offset = (unsigned)(end - s) + 1;
    if (size > rem
        || size <= offset + 1
        || offset != i + 1
        || s[size - 1] != '\n')
      return false;

    for (i = offset; i < size; i++)
      if (s[i] == 0)
        return false;

    for (i = offset; i < size - 1; i++)
      if (s[i] == '=')
        break;
    if (i == size - 1)
      return false;

    name.SetFrom(s + offset, i - offset);
    val.SetFrom(s + i + 1, (unsigned)(size - 1 - (i + 1)));
    
    bool parsed = false;
    if (isFile)
    {
      bool isDetectedName = true;
      // only lower case (name) is supported
      if (name.IsEqualTo("path"))
      {
        if (Path_Defined)
          DoubleTagError = true;
        Path = val;
        Path_Defined = true;
        parsed = true;
      }
      else if (name.IsEqualTo("linkpath"))
      {
        if (Link_Defined)
          DoubleTagError = true;
        Link = val;
        Link_Defined = true;
        parsed = true;
      }
      else if (name.IsEqualTo("uname"))
      {
        if (User_Defined)
          DoubleTagError = true;
        User = val;
        User_Defined = true;
        parsed = true;
      }
      else if (name.IsEqualTo("gname"))
      {
        if (Group_Defined)
          DoubleTagError = true;
        Group = val;
        Group_Defined = true;
        parsed = true;
      }
      else if (name.IsEqualTo("uid"))
      {
        parsed = ParseID(val, UID_Defined, UID);
      }
      else if (name.IsEqualTo("gid"))
      {
        parsed = ParseID(val, GID_Defined, GID);
      }
      else if (name.IsEqualTo("size"))
      {
        if (Size_Defined)
          DoubleTagError = true;
        Size_Defined = false;
        if (!val.IsEmpty())
        {
          const char *end2;
          Size = ConvertStringToUInt64(val.Ptr(), &end2);
          if (*end2 == 0)
          {
            Size_Defined = true;
            parsed = true;
          }
        }
      }
      else if (name.IsEqualTo("mtime"))
        { parsed = ParsePaxTime(val, MTime, DoubleTagError); }
      else if (name.IsEqualTo("atime"))
        { parsed = ParsePaxTime(val, ATime, DoubleTagError); }
      else if (name.IsEqualTo("ctime"))
        { parsed = ParsePaxTime(val, CTime, DoubleTagError); }
      else
        isDetectedName = false;
      if (isDetectedName && !parsed)
        TagParsingError = true;
    }
    if (!parsed)
    {
      if (!UnknownLines_Overflow)
      {
        const unsigned addSize = size - offset;
        if (UnknownLines.Len() + addSize < (1 << 16))
          UnknownLines.AddFrom(s + offset, addSize);
        else
          UnknownLines_Overflow = true;
      }
    }

    s += size;
    rem -= size;
  }
  return true;
}


HRESULT CArchive::ReadItem2(CItemEx &item)
{
  // CItem
  
  item.SparseBlocks.Clear();
  item.PaxTimes.Clear();

  // CItemEx

  item.HeaderSize = 0;
  item.Num_Pax_Records = 0;

  item.LongName_WasUsed = false;
  item.LongName_WasUsed_2 = false;

  item.LongLink_WasUsed = false;
  item.LongLink_WasUsed_2 = false;

  item.HeaderError = false;
  item.IsSignedChecksum = false;
  item.Prefix_WasUsed = false;
  
  item.Pax_Error = false;
  item.Pax_Overflow = false;
  item.pax_path_WasUsed = false;
  item.pax_link_WasUsed = false;
  item.pax_size_WasUsed = false;
  
  item.PaxExtra.Clear();

  item.EncodingCharacts.Clear();
  
  // CArchive temp variable

  NameBuf.Init();
  LinkBuf.Init();
  PaxBuf.Init();
  PaxBuf_global.Init();
  
  UInt64 numExtraRecords = 0;

  for (;;)
  {
    if (OpenCallback)
    {
      RINOK(Progress(item, 0))
    }

    RINOK(GetNextItemReal(item))

    // NumRecords++;

    if (!filled)
    {
      if (error == k_ErrorType_OK)
      if (numExtraRecords != 0
          || item.LongName_WasUsed
          || item.LongLink_WasUsed
          || item.Num_Pax_Records != 0)
        error = k_ErrorType_Corrupted;
      return S_OK;
    }
    if (error != k_ErrorType_OK)
      return S_OK;

    numExtraRecords++;

    const char lf = item.LinkFlag;
    if (lf == NFileHeader::NLinkFlag::kGnu_LongName ||
        lf == NFileHeader::NLinkFlag::kGnu_LongLink)
    {
      // GNU tar ignores item.Name after LinkFlag test
      // 22.00 : now we also ignore item.Name here
      /*
      if (item.Name != NFileHeader::kLongLink &&
          item.Name != NFileHeader::kLongLink2)
      {
        break;
        // return S_OK;
      }
      */

      CTempBuffer *tb =
          lf == NFileHeader::NLinkFlag::kGnu_LongName ?
          &NameBuf :
          &LinkBuf;
      
      /*
      if (item.PackSize > (1 << 29))
      {
        // break;
        return S_OK;
      }
      */

      const unsigned kLongNameSizeMax = (unsigned)1 << 14;
      RINOK(ReadDataToBuffer(item, *tb, kLongNameSizeMax))
      if (error != k_ErrorType_OK)
        return S_OK;

      if (lf == NFileHeader::NLinkFlag::kGnu_LongName)
      {
        item.LongName_WasUsed_2 =
        item.LongName_WasUsed;
        item.LongName_WasUsed = true;
      }
      else
      {
        item.LongLink_WasUsed_2 =
        item.LongLink_WasUsed;
        item.LongLink_WasUsed = true;
      }

      if (!tb->StringSize_IsConfirmed)
        tb->StringSize = 0;
      item.HeaderSize += item.Get_PackSize_Aligned();
      if (tb->StringSize == 0 ||
          tb->StringSize + 1 != item.PackSize)
        item.HeaderError = true;
      if (tb->IsNonZeroTail)
        item.HeaderError = true;
      continue;
    }

    if (lf == NFileHeader::NLinkFlag::kGlobal ||
        lf == NFileHeader::NLinkFlag::kPax ||
        lf == NFileHeader::NLinkFlag::kPax_2)
    {
      // GNU tar ignores item.Name after LinkFlag test
      // 22.00 : now we also ignore item.Name here
      /*
      if (item.PackSize > (UInt32)1 << 26)
      {
        break; // we don't want big PaxBuf files
        // return S_OK;
      }
      */
      const unsigned kParsingPaxSizeMax = (unsigned)1 << 26;

      const bool isStartHeader = (item.HeaderSize == NFileHeader::kRecordSize);

      CTempBuffer *tb = (lf == NFileHeader::NLinkFlag::kGlobal ? &PaxBuf_global : &PaxBuf);

      RINOK(ReadDataToBuffer(item, *tb, kParsingPaxSizeMax))
      if (error != k_ErrorType_OK)
        return S_OK;

      item.HeaderSize += item.Get_PackSize_Aligned();

      if (tb->StringSize != item.PackSize
          || tb->StringSize == 0
          || tb->IsNonZeroTail)
        item.Pax_Error = true;

      item.Num_Pax_Records++;
      if (lf != NFileHeader::NLinkFlag::kGlobal)
      {
        item.PaxExtra.RecordPath = item.Name;
        continue;
      }
      // break; // for debug
      {
        if (PaxGlobal_Defined)
          _is_PaxGlobal_Error = true;
        CPaxInfo paxInfo;
        if (paxInfo.ParsePax(PaxBuf_global, false))
        {
          PaxGlobal.RawLines = paxInfo.UnknownLines;
          PaxGlobal.RecordPath = item.Name;
          PaxGlobal_Defined = true;
        }
        else
          _is_PaxGlobal_Error = true;
        
        if (isStartHeader
            && item.Num_Pax_Records == 1
            && numExtraRecords == 1)
        {
          // we skip global pax header info after parsing
          item.HeaderPos += item.HeaderSize;
          item.HeaderSize = 0;
          item.Num_Pax_Records = 0;
          numExtraRecords = 0;
        }
        else
          _is_PaxGlobal_Error = true;
      }
      continue;
    }
    
    /*
    if (lf == NFileHeader::NLinkFlag::kDumpDir ||
        lf == NFileHeader::NLinkFlag::kSparse)
    {
      // GNU Extensions to the Archive Format
      break;
    }
    if (lf > '7' || (lf < '0' && lf != 0))
    {
      break;
      // return S_OK;
    }
    */
    break;
  }
    
  // we still use name from main header, if long_name is bad
  if (item.LongName_WasUsed && NameBuf.StringSize != 0)
  {
    NameBuf.CopyToString(item.Name);
    // item.Name_CouldBeReduced = false;
  }
  
  if (item.LongLink_WasUsed)
  {
    // we use empty link, if long_link is bad
    LinkBuf.CopyToString(item.LinkName);
    // item.LinkName_CouldBeReduced = false;
  }
  
  error = k_ErrorType_OK;
  
  if (PaxBuf.StringSize != 0)
  {
    CPaxInfo paxInfo;
    if (!paxInfo.ParsePax(PaxBuf, true))
      item.Pax_Error = true;
    else
    {
      if (paxInfo.Path_Defined) //  if (!paxInfo.Path.IsEmpty())
      {
        item.Name = paxInfo.Path;
        item.pax_path_WasUsed = true;
      }
      if (paxInfo.Link_Defined) // (!paxInfo.Link.IsEmpty())
      {
        item.LinkName = paxInfo.Link;
        item.pax_link_WasUsed = true;
      }
      if (paxInfo.User_Defined)
      {
        item.User = paxInfo.User;
        // item.pax_uname_WasUsed = true;
      }
      if (paxInfo.Group_Defined)
      {
        item.Group = paxInfo.Group;
        // item.pax_gname_WasUsed = true;
      }
      if (paxInfo.UID_Defined)
      {
        item.UID = (UInt32)paxInfo.UID;
      }
      if (paxInfo.GID_Defined)
      {
        item.GID = (UInt32)paxInfo.GID;
      }
      
      if (paxInfo.Size_Defined)
      {
        const UInt64 piSize = paxInfo.Size;
        // GNU TAR ignores (item.Size) in that case
        if (item.Size != 0 && item.Size != piSize)
          item.Pax_Error = true;
        item.Size = piSize;
        item.PackSize = piSize;
        item.pax_size_WasUsed = true;
      }
      
      item.PaxTimes = paxInfo;
      item.PaxExtra.RawLines = paxInfo.UnknownLines;
      if (paxInfo.UnknownLines_Overflow)
        item.Pax_Overflow = true;
      if (paxInfo.TagParsingError)
        item.Pax_Error = true;
      if (paxInfo.DoubleTagError)
        item.Pax_Error = true;
    }
  }

  return S_OK;
}



HRESULT CArchive::ReadItem(CItemEx &item)
{
  item.HeaderPos = _phySize;
  
  const HRESULT res = ReadItem2(item);
  
  /*
  if (error == k_ErrorType_Warning)
    _is_Warning = true;
  else
  */
  
  if (error != k_ErrorType_OK)
    _error = error;
  
  RINOK(res)

  if (filled)
  {
    if (item.IsMagic_GNU())
      _are_Gnu = true;
    else if (item.IsMagic_Posix_ustar_00())
      _are_Posix = true;
    
    if (item.Num_Pax_Records != 0)
      _are_Pax = true;

    if (item.PaxTimes.MTime.IsDefined())  _are_mtime = true;
    if (item.PaxTimes.ATime.IsDefined())  _are_atime = true;
    if (item.PaxTimes.CTime.IsDefined())  _are_ctime = true;

    if (item.pax_path_WasUsed)
      _are_pax_path = true;
    if (item.pax_link_WasUsed)
      _are_pax_link = true;
    if (item.LongName_WasUsed)
      _are_LongName = true;
    if (item.LongLink_WasUsed)
      _are_LongLink = true;
    if (item.Prefix_WasUsed)
      _pathPrefix_WasUsed = true;
    /*
    if (item.IsSparse())
      _isSparse = true;
    */
    if (item.Is_PaxExtendedHeader())
      _are_Pax_Items = true;
    if (item.IsThereWarning()
        || item.HeaderError
        || item.Pax_Error)
      _is_Warning = true;
  }

  const UInt64 headerEnd = item.HeaderPos + item.HeaderSize;
  // _headersSize += headerEnd - _phySize;
  // we don't count skipped records
  _headersSize += item.HeaderSize;
  _phySize = headerEnd;
  return S_OK;
}

}}
