// TarIn.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/StringToInt.h"

#include "../../Common/StreamUtils.h"

#include "../IArchive.h"

#include "TarIn.h"

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

static bool OctalToNumber32(const char *srcString, unsigned size, UInt32 &res, bool allowEmpty = false)
{
  UInt64 res64;
  if (!OctalToNumber(srcString, size, res64, allowEmpty))
    return false;
  res = (UInt32)res64;
  return (res64 <= 0xFFFFFFFF);
}

#define RIF(x) { if (!(x)) return S_OK; }

/*
static bool IsEmptyData(const char *buf, size_t size)
{
  for (unsigned i = 0; i < size; i++)
    if (buf[i] != 0)
      return false;
  return true;
}
*/

static bool IsRecordLast(const char *buf)
{
  for (unsigned i = 0; i < NFileHeader::kRecordSize; i++)
    if (buf[i] != 0)
      return false;
  return true;
}

static void ReadString(const char *s, unsigned size, AString &result)
{
  char temp[NFileHeader::kRecordSize + 1];
  MyStrNCpy(temp, s, size);
  temp[size] = '\0';
  result = temp;
}

static bool ParseInt64(const char *p, Int64 &val)
{
  UInt32 h = GetBe32(p);
  val = (Int64)GetBe64(p + 4);
  if (h == (UInt32)1 << 31)
    return ((val >> 63) & 1) == 0;
  if (h == (UInt32)(Int32)-1)
    return ((val >> 63) & 1) != 0;
  UInt64 uv;
  bool res = OctalToNumber(p, 12, uv);
  val = (Int64)uv;
  return res;
}

static bool ParseInt64_MTime(const char *p, Int64 &val)
{
  // rare case tar : ZEROs in Docker-Windows TARs
  // rare case tar : spaces
  if (GetUi32(p) != 0)
  for (unsigned i = 0; i < 12; i++)
    if (p[i] != ' ')
      return ParseInt64(p, val);
  val = 0;
  return true;
}

static bool ParseSize(const char *p, UInt64 &val)
{
  if (GetBe32(p) == (UInt32)1 << 31)
  {
    // GNU extension
    val = GetBe64(p + 4);
    return ((val >> 63) & 1) == 0;
  }
  return OctalToNumber(p, 12, val,
      true // 20.03: allow empty size for 'V' Label entry
      );
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
  CHECK(OctalToNumber32(p, 8, mode, true)); p += 8;

  // if (!OctalToNumber32(p, 8, item.UID)) item.UID = 0;
  p += 8;
  // if (!OctalToNumber32(p, 8, item.GID)) item.GID = 0;
  p += 8;

  UInt64 packSize;
  Int64 time;
  UInt32 checkSum;
  CHECK(ParseSize(p, packSize)); p += 12;
  CHECK(ParseInt64_MTime(p, time)); p += 12;
  CHECK(OctalToNumber32(p, 8, checkSum));
  return k_IsArc_Res_YES;
}

static HRESULT GetNextItemReal(ISequentialInStream *stream, bool &filled, CItemEx &item, EErrorType &error)
{
  char buf[NFileHeader::kRecordSize];
  char *p = buf;

  error = k_ErrorType_OK;
  filled = false;

  bool thereAreEmptyRecords = false;
  for (;;)
  {
    size_t processedSize = NFileHeader::kRecordSize;
    RINOK(ReadStream(stream, buf, &processedSize));
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
    if (!IsRecordLast(buf))
      break;
    item.HeaderSize += NFileHeader::kRecordSize;
    thereAreEmptyRecords = true;
  }
  if (thereAreEmptyRecords)
  {
    // error = "There are data after end of archive";
    return S_OK;
  }
  
  error = k_ErrorType_Corrupted;
  ReadString(p, NFileHeader::kNameSize, item.Name); p += NFileHeader::kNameSize;
  item.NameCouldBeReduced =
      (item.Name.Len() == NFileHeader::kNameSize ||
       item.Name.Len() == NFileHeader::kNameSize - 1);

  // we allow empty Mode value for LongName prefix items
  RIF(OctalToNumber32(p, 8, item.Mode, true)); p += 8;

  if (!OctalToNumber32(p, 8, item.UID)) { item.UID = 0; }  p += 8;
  if (!OctalToNumber32(p, 8, item.GID)) { item.GID = 0; }  p += 8;

  RIF(ParseSize(p, item.PackSize));
  item.Size = item.PackSize;
  p += 12;
  RIF(ParseInt64_MTime(p, item.MTime)); p += 12;
  
  UInt32 checkSum;
  RIF(OctalToNumber32(p, 8, checkSum));
  memset(p, ' ', 8); p += 8;

  item.LinkFlag = *p++;

  ReadString(p, NFileHeader::kNameSize, item.LinkName); p += NFileHeader::kNameSize;
  item.LinkNameCouldBeReduced =
      (item.LinkName.Len() == NFileHeader::kNameSize ||
       item.LinkName.Len() == NFileHeader::kNameSize - 1);

  memcpy(item.Magic, p, 8); p += 8;

  ReadString(p, NFileHeader::kUserNameSize, item.User); p += NFileHeader::kUserNameSize;
  ReadString(p, NFileHeader::kGroupNameSize, item.Group); p += NFileHeader::kGroupNameSize;

  item.DeviceMajorDefined = (p[0] != 0); if (item.DeviceMajorDefined) { RIF(OctalToNumber32(p, 8, item.DeviceMajor)); } p += 8;
  item.DeviceMinorDefined = (p[0] != 0); if (item.DeviceMinorDefined) { RIF(OctalToNumber32(p, 8, item.DeviceMinor)); } p += 8;

  if (p[0] != 0)
  {
    AString prefix;
    ReadString(p, NFileHeader::kPrefixSize, prefix);
    if (!prefix.IsEmpty()
        && item.IsUstarMagic()
        && (item.LinkFlag != 'L' /* || prefix != "00000000000" */ ))
      item.Name = prefix + '/' + item.Name;
  }

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
    But some TAR programs use sum of signed byte values.
    So we check both values.
  */
  UInt32 checkSumReal = 0;
  Int32 checkSumReal_Signed = 0;
  for (unsigned i = 0; i < NFileHeader::kRecordSize; i++)
  {
    char c = buf[i];
    checkSumReal_Signed += (signed char)c;
    checkSumReal += (Byte)buf[i];
  }
  
  if (checkSumReal != checkSum)
  {
    if ((UInt32)checkSumReal_Signed != checkSum)
      return S_OK;
  }

  item.HeaderSize += NFileHeader::kRecordSize;

  if (item.LinkFlag == NFileHeader::NLinkFlag::kSparse)
  {
    Byte isExtended = (Byte)buf[482];
    if (isExtended != 0 && isExtended != 1)
      return S_OK;
    RIF(ParseSize(buf + 483, item.Size));
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
      RIF(ParseSize(p, sb.Offset));
      RIF(ParseSize(p + 12, sb.Size));
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
      RINOK(ReadStream(stream, buf, &processedSize));
      if (processedSize != NFileHeader::kRecordSize)
      {
        error = k_ErrorType_UnexpectedEnd;
        return S_OK;
      }

      item.HeaderSize += NFileHeader::kRecordSize;
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
        RIF(ParseSize(p, sb.Offset));
        RIF(ParseSize(p + 12, sb.Size));
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
 
  filled = true;
  error = k_ErrorType_OK;
  return S_OK;
}


static HRESULT ReadDataToString(ISequentialInStream *stream, CItemEx &item, AString &s, EErrorType &error)
{
  const unsigned packSize = (unsigned)item.GetPackSizeAligned();
  size_t processedSize = packSize;
  HRESULT res = ReadStream(stream, s.GetBuf(packSize), &processedSize);
  item.HeaderSize += (unsigned)processedSize;
  s.ReleaseBuf_CalcLen((unsigned)item.PackSize);
  RINOK(res);
  if (processedSize != packSize)
    error = k_ErrorType_UnexpectedEnd;
  return S_OK;
}

static bool ParsePaxLongName(const AString &src, AString &dest)
{
  dest.Empty();
  for (unsigned pos = 0;;)
  {
    if (pos >= src.Len())
      return false;
    const char *start = src.Ptr(pos);
    const char *end;
    const UInt32 lineLen = ConvertStringToUInt32(start, &end);
    if (end == start)
      return false;
    if (*end != ' ')
      return false;
    if (lineLen > src.Len() - pos)
      return false;
    unsigned offset = (unsigned)(end - start) + 1;
    if (lineLen < offset)
      return false;
    if (IsString1PrefixedByString2(src.Ptr(pos + offset), "path="))
    {
      offset += 5; // "path="
      dest = src.Mid(pos + offset, lineLen - offset);
      if (dest.IsEmpty())
        return false;
      if (dest.Back() != '\n')
        return false;
      dest.DeleteBack();
      return true;
    }
    pos += lineLen;
  }
}

HRESULT ReadItem(ISequentialInStream *stream, bool &filled, CItemEx &item, EErrorType &error)
{
  item.HeaderSize = 0;

  bool flagL = false;
  bool flagK = false;
  AString nameL;
  AString nameK;
  AString pax;
  
  for (;;)
  {
    RINOK(GetNextItemReal(stream, filled, item, error));
    if (!filled)
    {
      if (error == k_ErrorType_OK && (flagL || flagK))
        error = k_ErrorType_Corrupted;
      return S_OK;
    }
    
    if (error != k_ErrorType_OK)
      return S_OK;
    
    if (item.LinkFlag == NFileHeader::NLinkFlag::kGnu_LongName || // file contains a long name
        item.LinkFlag == NFileHeader::NLinkFlag::kGnu_LongLink)   // file contains a long linkname
    {
      AString *name;
      if (item.LinkFlag == NFileHeader::NLinkFlag::kGnu_LongName)
        { if (flagL) return S_OK; flagL = true; name = &nameL; }
      else
        { if (flagK) return S_OK; flagK = true; name = &nameK; }

      if (item.Name != NFileHeader::kLongLink &&
          item.Name != NFileHeader::kLongLink2)
        return S_OK;
      if (item.PackSize > (1 << 14))
        return S_OK;

      RINOK(ReadDataToString(stream, item, *name, error));
      if (error != k_ErrorType_OK)
        return S_OK;

      continue;
    }

    switch (item.LinkFlag)
    {
      case 'g':
      case 'x':
      case 'X':
      {
        const char *s = item.Name.Ptr();
        if (IsString1PrefixedByString2(s, "./"))
          s += 2;
        if (IsString1PrefixedByString2(s, "./"))
          s += 2;
        if (   IsString1PrefixedByString2(s, "PaxHeader/")
            || IsString1PrefixedByString2(s, "PaxHeaders.X/")
            || IsString1PrefixedByString2(s, "PaxHeaders.4467/")
            || StringsAreEqual_Ascii(s, "@PaxHeader")
            )
        {
          RINOK(ReadDataToString(stream, item, pax, error));
          if (error != k_ErrorType_OK)
            return S_OK;
          continue;
        }
        break;
      }
      case NFileHeader::NLinkFlag::kDumpDir:
      {
        break;
        // GNU Extensions to the Archive Format
      }
      case NFileHeader::NLinkFlag::kSparse:
      {
        break;
        // GNU Extensions to the Archive Format
      }
      default:
        if (item.LinkFlag > '7' || (item.LinkFlag < '0' && item.LinkFlag != 0))
          return S_OK;
    }
    
    if (flagL)
    {
      item.Name = nameL;
      item.NameCouldBeReduced = false;
    }
    
    if (flagK)
    {
      item.LinkName = nameK;
      item.LinkNameCouldBeReduced = false;
    }

    error = k_ErrorType_OK;

    if (!pax.IsEmpty())
    {
      AString name;
      if (ParsePaxLongName(pax, name))
        item.Name = name;
      else
      {
        // no "path" property is allowed in pax4467
        // error = k_ErrorType_Warning;
      }
      pax.Empty();
    }

    return S_OK;
  }
}

}}
