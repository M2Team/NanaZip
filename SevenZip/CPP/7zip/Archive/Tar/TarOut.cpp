// TarOut.cpp

#include "StdAfx.h"

#include "../../Common/StreamUtils.h"

#include "TarOut.h"

namespace NArchive {
namespace NTar {

HRESULT COutArchive::WriteBytes(const void *data, unsigned size)
{
  Pos += size;
  return WriteStream(m_Stream, data, size);
}

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

static bool WriteOctal_8(char *s, UInt32 val)
{
  const unsigned kNumDigits = 8 - 1;
  if (val >= ((UInt32)1 << (kNumDigits * 3)))
    return false;
  for (unsigned i = 0; i < kNumDigits; i++)
  {
    s[kNumDigits - 1 - i] = (char)('0' + (val & 7));
    val >>= 3;
  }
  return true;
}

static void WriteOctal_12(char *s, UInt64 val)
{
  const unsigned kNumDigits = 12 - 1;
  if (val >= ((UInt64)1 << (kNumDigits * 3)))
  {
    // GNU extension;
    s[0] = (char)(Byte)0x80;
    s[1] = s[2] = s[3] = 0;
    for (unsigned i = 0; i < 8; i++, val <<= 8)
      s[4 + i] = (char)(val >> 56);
    return;
  }
  for (unsigned i = 0; i < kNumDigits; i++)
  {
    s[kNumDigits - 1 - i] = (char)('0' + (val & 7));
    val >>= 3;
  }
}

static void WriteOctal_12_Signed(char *s, Int64 val)
{
  if (val >= 0)
  {
    WriteOctal_12(s, (UInt64)val);
    return;
  }
  s[0] = s[1] = s[2] = s[3] = (char)(Byte)0xFF;
  for (unsigned i = 0; i < 8; i++, val <<= 8)
    s[4 + i] = (char)(val >> 56);
}

static bool CopyString(char *dest, const AString &src, unsigned maxSize)
{
  if (src.Len() >= maxSize)
    return false;
  MyStringCopy(dest, (const char *)src);
  return true;
}

#define RETURN_IF_NOT_TRUE(x) { if (!(x)) return E_FAIL; }

HRESULT COutArchive::WriteHeaderReal(const CItem &item)
{
  char record[NFileHeader::kRecordSize];
  memset(record, 0, NFileHeader::kRecordSize);
  char *cur = record;

  if (item.Name.Len() > NFileHeader::kNameSize)
    return E_FAIL;
  MyStrNCpy(cur, item.Name, NFileHeader::kNameSize);
  cur += NFileHeader::kNameSize;

  RETURN_IF_NOT_TRUE(WriteOctal_8(cur, item.Mode)); cur += 8;
  RETURN_IF_NOT_TRUE(WriteOctal_8(cur, item.UID)); cur += 8;
  RETURN_IF_NOT_TRUE(WriteOctal_8(cur, item.GID)); cur += 8;

  WriteOctal_12(cur, item.PackSize); cur += 12;
  WriteOctal_12_Signed(cur, item.MTime); cur += 12;
  
  memset(cur, ' ', 8);
  cur += 8;

  *cur++ = item.LinkFlag;

  RETURN_IF_NOT_TRUE(CopyString(cur, item.LinkName, NFileHeader::kNameSize));
  cur += NFileHeader::kNameSize;

  memcpy(cur, item.Magic, 8);
  cur += 8;

  RETURN_IF_NOT_TRUE(CopyString(cur, item.User, NFileHeader::kUserNameSize));
  cur += NFileHeader::kUserNameSize;
  RETURN_IF_NOT_TRUE(CopyString(cur, item.Group, NFileHeader::kGroupNameSize));
  cur += NFileHeader::kGroupNameSize;


  if (item.DeviceMajorDefined) RETURN_IF_NOT_TRUE(WriteOctal_8(cur, item.DeviceMajor)); cur += 8;
  if (item.DeviceMinorDefined) RETURN_IF_NOT_TRUE(WriteOctal_8(cur, item.DeviceMinor)); cur += 8;

  if (item.IsSparse())
  {
    record[482] = (char)(item.SparseBlocks.Size() > 4 ? 1 : 0);
    WriteOctal_12(record + 483, item.Size);
    for (unsigned i = 0; i < item.SparseBlocks.Size() && i < 4; i++)
    {
      const CSparseBlock &sb = item.SparseBlocks[i];
      char *p = record + 386 + 24 * i;
      WriteOctal_12(p, sb.Offset);
      WriteOctal_12(p + 12, sb.Size);
    }
  }

  {
    UInt32 checkSum = 0;
    {
      for (unsigned i = 0; i < NFileHeader::kRecordSize; i++)
        checkSum += (Byte)record[i];
    }
    /* we use GNU TAR scheme:
       checksum field is formatted differently from the
       other fields: it has [6] digits, a null, then a space. */
    // RETURN_IF_NOT_TRUE(WriteOctal_8(record + 148, checkSum));
    const unsigned kNumDigits = 6;
    for (unsigned i = 0; i < kNumDigits; i++)
    {
      record[148 + kNumDigits - 1 - i] = (char)('0' + (checkSum & 7));
      checkSum >>= 3;
    }
    record[148 + 6] = 0;
  }

  RINOK(WriteBytes(record, NFileHeader::kRecordSize));

  if (item.IsSparse())
  {
    for (unsigned i = 4; i < item.SparseBlocks.Size();)
    {
      memset(record, 0, NFileHeader::kRecordSize);
      for (unsigned t = 0; t < 21 && i < item.SparseBlocks.Size(); t++, i++)
      {
        const CSparseBlock &sb = item.SparseBlocks[i];
        char *p = record + 24 * t;
        WriteOctal_12(p, sb.Offset);
        WriteOctal_12(p + 12, sb.Size);
      }
      record[21 * 24] = (char)(i < item.SparseBlocks.Size() ? 1 : 0);
      RINOK(WriteBytes(record, NFileHeader::kRecordSize));
    }
  }

  return S_OK;
}

HRESULT COutArchive::WriteHeader(const CItem &item)
{
  unsigned nameSize = item.Name.Len();
  unsigned linkSize = item.LinkName.Len();

  /* There two versions of GNU tar:
    OLDGNU_FORMAT: it writes short name and zero at the  end
    GNU_FORMAT:    it writes only short name without zero at the end
    we write it as OLDGNU_FORMAT with zero at the end */

  if (nameSize < NFileHeader::kNameSize &&
      linkSize < NFileHeader::kNameSize)
    return WriteHeaderReal(item);

  CItem mi = item;
  mi.Name = NFileHeader::kLongLink;
  mi.LinkName.Empty();
  for (int i = 0; i < 2; i++)
  {
    const AString *name;
    // We suppose that GNU tar also writes item for long link before item for LongName?
    if (i == 0)
    {
      mi.LinkFlag = NFileHeader::NLinkFlag::kGnu_LongLink;
      name = &item.LinkName;
    }
    else
    {
      mi.LinkFlag = NFileHeader::NLinkFlag::kGnu_LongName;
      name = &item.Name;
    }
    if (name->Len() < NFileHeader::kNameSize)
      continue;
    unsigned nameStreamSize = name->Len() + 1;
    mi.PackSize = nameStreamSize;
    RINOK(WriteHeaderReal(mi));
    RINOK(WriteBytes((const char *)*name, nameStreamSize));
    RINOK(FillDataResidual(nameStreamSize));
  }

  mi = item;
  if (mi.Name.Len() >= NFileHeader::kNameSize)
    mi.Name.SetFrom(item.Name, NFileHeader::kNameSize - 1);
  if (mi.LinkName.Len() >= NFileHeader::kNameSize)
    mi.LinkName.SetFrom(item.LinkName, NFileHeader::kNameSize - 1);
  return WriteHeaderReal(mi);
}

HRESULT COutArchive::FillDataResidual(UInt64 dataSize)
{
  unsigned lastRecordSize = ((unsigned)dataSize & (NFileHeader::kRecordSize - 1));
  if (lastRecordSize == 0)
    return S_OK;
  unsigned rem = NFileHeader::kRecordSize - lastRecordSize;
  Byte buf[NFileHeader::kRecordSize];
  memset(buf, 0, rem);
  return WriteBytes(buf, rem);
}

HRESULT COutArchive::WriteFinishHeader()
{
  Byte record[NFileHeader::kRecordSize];
  memset(record, 0, NFileHeader::kRecordSize);
  for (unsigned i = 0; i < 2; i++)
  {
    RINOK(WriteBytes(record, NFileHeader::kRecordSize));
  }
  return S_OK;
}

}}
