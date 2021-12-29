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

static void WriteBin_64bit(char *s, UInt64 val)
{
  for (unsigned i = 0; i < 8; i++, val <<= 8)
    s[i] = (char)(val >> 56);
}

static void WriteOctal_12(char *s, UInt64 val)
{
  const unsigned kNumDigits = 12 - 1;
  if (val >= ((UInt64)1 << (kNumDigits * 3)))
  {
    // GNU extension;
    s[0] = (char)(Byte)0x80;
    s[1] = s[2] = s[3] = 0;
    WriteBin_64bit(s + 4, val);
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
  WriteBin_64bit(s + 4, val);
}

static void CopyString(char *dest, const AString &src, unsigned maxSize)
{
  unsigned len = src.Len();
  if (len == 0)
    return;
  // 21.07: we don't require additional 0 character at the end
  if (len > maxSize)
  {
    len = maxSize;
    // return false;
  }
  memcpy(dest, src.Ptr(), len);
  // return true;
}

#define RETURN_IF_NOT_TRUE(x) { if (!(x)) return E_FAIL; }

#define COPY_STRING_CHECK(dest, src, size) \
    CopyString(dest, src, size);   dest += (size);

#define WRITE_OCTAL_8_CHECK(dest, src) \
    RETURN_IF_NOT_TRUE(WriteOctal_8(dest, src));
    

HRESULT COutArchive::WriteHeaderReal(const CItem &item)
{
  char record[NFileHeader::kRecordSize];
  memset(record, 0, NFileHeader::kRecordSize);
  char *cur = record;

  COPY_STRING_CHECK (cur, item.Name, NFileHeader::kNameSize);

  WRITE_OCTAL_8_CHECK (cur, item.Mode);  cur += 8;
  WRITE_OCTAL_8_CHECK (cur, item.UID);   cur += 8;
  WRITE_OCTAL_8_CHECK (cur, item.GID);   cur += 8;

  WriteOctal_12(cur, item.PackSize); cur += 12;
  WriteOctal_12_Signed(cur, item.MTime); cur += 12;
  
  memset(cur, ' ', 8); // checksum field
  cur += 8;

  *cur++ = item.LinkFlag;

  COPY_STRING_CHECK (cur, item.LinkName, NFileHeader::kNameSize);

  memcpy(cur, item.Magic, 8);
  cur += 8;

  COPY_STRING_CHECK (cur, item.User, NFileHeader::kUserNameSize);
  COPY_STRING_CHECK (cur, item.Group, NFileHeader::kGroupNameSize);

  if (item.DeviceMajorDefined)
    WRITE_OCTAL_8_CHECK (cur, item.DeviceMajor);
  cur += 8;
  if (item.DeviceMinorDefined)
    WRITE_OCTAL_8_CHECK (cur, item.DeviceMinor);
  cur += 8;

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
    // WRITE_OCTAL_8_CHECK(record + 148, checkSum);
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


/* OLD_GNU_TAR: writes short name with    zero at the end
   NEW_GNU_TAR: writes short name without zero at the end */

static const unsigned kNameSize_Max =
    NFileHeader::kNameSize;     // NEW_GNU_TAR / 7-Zip 21.07
    // NFileHeader::kNameSize - 1; // OLD_GNU_TAR / old 7-Zip

#define DOES_NAME_FIT_IN_FIELD(name) ((name).Len() <= kNameSize_Max)

HRESULT COutArchive::WriteHeader(const CItem &item)
{
  if (DOES_NAME_FIT_IN_FIELD(item.Name) &&
      DOES_NAME_FIT_IN_FIELD(item.LinkName))
    return WriteHeaderReal(item);

  // here we can get all fields from main (item) or create new empty item
  /*
  CItem mi;
  mi.SetDefaultWriteFields();
  */
  
  CItem mi = item;
  mi.LinkName.Empty();
  // SparseBlocks will be ignored by IsSparse()
  // mi.SparseBlocks.Clear();

  mi.Name = NFileHeader::kLongLink;
  // 21.07 : we set Mode and MTime props as in GNU TAR:
  mi.Mode = 0644; // octal
  mi.MTime = 0;

  for (int i = 0; i < 2; i++)
  {
    const AString *name;
    // We suppose that GNU TAR also writes item for long link before item for LongName?
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
    if (DOES_NAME_FIT_IN_FIELD(*name))
      continue;
    // GNU TAR writes null character after NAME to file. We do same here:
    const unsigned nameStreamSize = name->Len() + 1;
    mi.PackSize = nameStreamSize;
    RINOK(WriteHeaderReal(mi));
    RINOK(WriteBytes(name->Ptr(), nameStreamSize));
    RINOK(FillDataResidual(nameStreamSize));
  }

  // 21.07: WriteHeaderReal() writes short part of (Name) and (LinkName).
  return WriteHeaderReal(item);
  /*
  mi = item;
  if (!DOES_NAME_FIT_IN_FIELD(mi.Name))
    mi.Name.SetFrom(item.Name, kNameSize_Max);
  if (!DOES_NAME_FIT_IN_FIELD(mi.LinkName))
    mi.LinkName.SetFrom(item.LinkName, kNameSize_Max);
  return WriteHeaderReal(mi);
  */
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

  const unsigned kNumFinishRecords = 2;

  /* GNU TAR by default uses --blocking-factor=20 (512 * 20 = 10 KiB)
     we also can use cluster alignment:
  const unsigned numBlocks = (unsigned)(Pos / NFileHeader::kRecordSize) + kNumFinishRecords;
  const unsigned kNumClusterBlocks = (1 << 3); // 8 blocks = 4 KiB
  const unsigned numFinishRecords = kNumFinishRecords + ((kNumClusterBlocks - numBlocks) & (kNumClusterBlocks - 1));
  */
  
  for (unsigned i = 0; i < kNumFinishRecords; i++)
  {
    RINOK(WriteBytes(record, NFileHeader::kRecordSize));
  }
  return S_OK;
}

}}
