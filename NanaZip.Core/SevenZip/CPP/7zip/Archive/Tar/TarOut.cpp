// TarOut.cpp

#include "StdAfx.h"

#include "../../../../C/7zCrc.h"

#include "../../../Common/IntToString.h"

#include "../../Common/StreamUtils.h"

#include "TarOut.h"

namespace NArchive {
namespace NTar {

using namespace NFileHeader;

// it's path prefix assigned by 7-Zip to show that file path was cut
#define K_PREFIX_PATH_CUT "@PathCut"

static const UInt32 k_7_oct_digits_Val_Max = ((UInt32)1 << (7 * 3)) - 1;

static void WriteOctal_8(char *s, UInt32 val)
{
  const unsigned kNumDigits = 8 - 1;
  if (val >= ((UInt32)1 << (kNumDigits * 3)))
  {
    val = 0;
    // return false;
  }
  for (unsigned i = 0; i < kNumDigits; i++)
  {
    s[kNumDigits - 1 - i] = (char)('0' + (val & 7));
    val >>= 3;
  }
  // return true;
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

static void WriteOctal_12_Signed(char *s, const Int64 val)
{
  if (val >= 0)
  {
    WriteOctal_12(s, (UInt64)val);
    return;
  }
  s[0] = s[1] = s[2] = s[3] = (char)(Byte)0xFF;
  WriteBin_64bit(s + 4, (UInt64)val);
}

static void CopyString(char *dest, const AString &src, const unsigned maxSize)
{
  unsigned len = src.Len();
  if (len == 0)
    return;
  // 21.07: new gnu : we don't require additional 0 character at the end
  // if (len >= maxSize)
  if (len > maxSize)
  {
    len = maxSize;
    /*
    // oldgnu needs 0 character at the end
    len = maxSize - 1;
    dest[len] = 0;
    */
  }
  memcpy(dest, src.Ptr(), len);
}

// #define RETURN_IF_NOT_TRUE(x) { if (!(x)) return E_INVALIDARG; }
#define RETURN_IF_NOT_TRUE(x) { x; }

#define COPY_STRING_CHECK(dest, src, size) \
    CopyString(dest, src, size);   dest += (size);

#define WRITE_OCTAL_8_CHECK(dest, src) \
    RETURN_IF_NOT_TRUE(WriteOctal_8(dest, src))
    

HRESULT COutArchive::WriteHeaderReal(const CItem &item, bool isPax
    // , bool zero_PackSize
    // , bool zero_MTime
    )
{
  /*
    if (isPax) { we don't use Glob_Name and Prefix }
    if (!isPax)
    {
      we use Glob_Name if it's not empty
      we use Prefix    if it's not empty
    }
  */
  char record[kRecordSize];
  memset(record, 0, kRecordSize);
  char *cur = record;

  COPY_STRING_CHECK (cur,
      (!isPax && !Glob_Name.IsEmpty()) ? Glob_Name : item.Name,
      kNameSize)

  WRITE_OCTAL_8_CHECK (cur, item.Mode)  cur += 8; // & k_7_oct_digits_Val_Max
  WRITE_OCTAL_8_CHECK (cur, item.UID)   cur += 8;
  WRITE_OCTAL_8_CHECK (cur, item.GID)   cur += 8;

  WriteOctal_12 (cur, /* zero_PackSize ? 0 : */ item.PackSize); cur += 12;
  WriteOctal_12_Signed (cur, /* zero_MTime ? 0 : */ item.MTime); cur += 12;
  
  // we will use binary init for checksum instead of memset
  // checksum field:
  // memset(cur, ' ', 8);
  cur += 8;

  *cur++ = item.LinkFlag;

  COPY_STRING_CHECK (cur, item.LinkName, kNameSize)

  memcpy(cur, item.Magic, 8);
  cur += 8;

  COPY_STRING_CHECK (cur, item.User, kUserNameSize)
  COPY_STRING_CHECK (cur, item.Group, kGroupNameSize)

  const bool needDevice = (IsPosixMode && !isPax);
  
  if (item.DeviceMajor_Defined)
    WRITE_OCTAL_8_CHECK (cur, item.DeviceMajor)
  else if (needDevice)
    WRITE_OCTAL_8_CHECK (cur, 0)
  cur += 8;
  
  if (item.DeviceMinor_Defined)
    WRITE_OCTAL_8_CHECK (cur, item.DeviceMinor)
  else if (needDevice)
    WRITE_OCTAL_8_CHECK (cur, 0)
  cur += 8;

  if (!isPax && !Prefix.IsEmpty())
  {
    COPY_STRING_CHECK (cur, Prefix, kPrefixSize)
  }

  if (item.Is_Sparse())
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
    UInt32 sum = (unsigned)(' ') * 8; // we use binary init
    {
      for (unsigned i = 0; i < kRecordSize; i++)
        sum += (Byte)record[i];
    }
    /* checksum field is formatted differently from the
       other fields: it has [6] digits, a null, then a space. */
    // WRITE_OCTAL_8_CHECK(record + 148, sum);
    const unsigned kNumDigits = 6;
    for (unsigned i = 0; i < kNumDigits; i++)
    {
      record[148 + kNumDigits - 1 - i] = (char)('0' + (sum & 7));
      sum >>= 3;
    }
    // record[148 + 6] = 0; // we need it, if we use memset(' ') init
    record[148 + 7] = ' '; // we need it, if we use binary init
  }

  RINOK(Write_Data(record, kRecordSize))

  if (item.Is_Sparse())
  {
    for (unsigned i = 4; i < item.SparseBlocks.Size();)
    {
      memset(record, 0, kRecordSize);
      for (unsigned t = 0; t < 21 && i < item.SparseBlocks.Size(); t++, i++)
      {
        const CSparseBlock &sb = item.SparseBlocks[i];
        char *p = record + 24 * t;
        WriteOctal_12(p, sb.Offset);
        WriteOctal_12(p + 12, sb.Size);
      }
      record[21 * 24] = (char)(i < item.SparseBlocks.Size() ? 1 : 0);
      RINOK(Write_Data(record, kRecordSize))
    }
  }

  return S_OK;
}


static void AddPaxLine(AString &s, const char *name, const AString &val)
{
  // s.Add_LF(); // for debug
  const unsigned len = 3 + (unsigned)strlen(name) + val.Len();
  AString n;
  for (unsigned numDigits = 1;; numDigits++)
  {
    n.Empty();
    n.Add_UInt32(numDigits + len);
    if (numDigits == n.Len())
      break;
  }
  s += n;
  s.Add_Space();
  s += name;
  s += '=';
  s += val;
  s.Add_LF();
}

// pt is defined : (pt.NumDigits >= 0)
static void AddPaxTime(AString &s, const char *name, const CPaxTime &pt,
    const CTimeOptions &options)
{
  unsigned numDigits = (unsigned)pt.NumDigits;
  if (numDigits > options.NumDigitsMax)
    numDigits = options.NumDigitsMax;

  bool needNs = false;
  UInt32 ns = 0;
  if (numDigits != 0)
  {
    ns = pt.Ns;
    // if (ns != 0) before reduction, we show all digits after digits reduction
    needNs = (ns != 0 || options.RemoveZeroMode == k_PaxTimeMode_DontRemoveZero);
    UInt32 d = 1;
    for (unsigned k = numDigits; k < 9; k++)
      d *= 10;
    ns /= d;
    ns *= d;
  }

  AString v;
  {
    Int64 sec = pt.Sec;
    if (pt.Sec < 0)
    {
      sec = -sec;
      v.Add_Minus();
      if (ns != 0)
      {
        ns = 1000*1000*1000 - ns;
        sec--;
      }
    }
    v.Add_UInt64((UInt64)sec);
  }
  
  if (needNs)
  {
    AString d;
    d.Add_UInt32(ns);
    while (d.Len() < 9)
      d.InsertAtFront('0');
    // here we have precision
    while (d.Len() > (unsigned)numDigits)
      d.DeleteBack();
    // GNU TAR reduces '0' digits.
    if (options.RemoveZeroMode == k_PaxTimeMode_RemoveZero_Always)
    while (!d.IsEmpty() && d.Back() == '0')
      d.DeleteBack();

    if (!d.IsEmpty())
    {
      v.Add_Dot();
      v += d;
      // v += "1234567009999"; // for debug
      // for (int y = 0; y < 1000; y++) v += '8'; // for debug
    }
  }

  AddPaxLine(s, name, v);
}


static void AddPax_UInt32_ifBig(AString &s, const char *name, const UInt32 &v)
{
  if (v > k_7_oct_digits_Val_Max)
  {
    AString s2;
    s2.Add_UInt32(v);
    AddPaxLine(s, name, s2);
  }
}
  

/* OLD_GNU_TAR: writes name with zero at the end
   NEW_GNU_TAR: can write name filled with all kNameSize characters */

static const unsigned kNameSize_Max =
    kNameSize;     // NEW_GNU_TAR / 7-Zip 21.07
    // kNameSize - 1; // OLD_GNU_TAR / old 7-Zip

#define DOES_NAME_FIT_IN_FIELD(name) ((name).Len() <= kNameSize_Max)


HRESULT COutArchive::WriteHeader(const CItem &item)
{
  Glob_Name.Empty();
  Prefix.Empty();

  unsigned namePos = 0;
  bool needPathCut = false;
  bool allowPrefix = false;

  if (!DOES_NAME_FIT_IN_FIELD(item.Name))
  {
    const char *s = item.Name;
    const char *p = s + item.Name.Len() - 1;
    for (; *p == '/' && p != s; p--)
      {}
    for (; p != s && p[-1] != '/'; p--)
      {}
    namePos = (unsigned)(p - s);
    needPathCut = true;
  }

  if (IsPosixMode)
  {
    AString s;
    
    if (needPathCut)
    {
      const unsigned nameLen = item.Name.Len() - namePos;
      if (   item.LinkFlag >= NLinkFlag::kNormal
          && item.LinkFlag <= NLinkFlag::kDirectory
          && namePos > 1
          && nameLen != 0
          // && IsPrefixAllowed
          && item.IsMagic_Posix_ustar_00())
      {
        /* GNU TAR decoder supports prefix field, only if (magic)
           signature matches 6-bytes "ustar\0".
           so here we use prefix field only in posix mode with posix signature */

        allowPrefix = true;
        // allowPrefix = false; // for debug
        if (namePos <= kPrefixSize + 1 && nameLen <= kNameSize_Max)
        {
          needPathCut = false;
          /* we will set Prefix and Glob_Name later, for such conditions:
             if (!DOES_NAME_FIT_IN_FIELD(item.Name) && !needPathCut) */
        }
      }

      if (needPathCut)
        AddPaxLine(s, "path", item.Name);
    }
    
    // AddPaxLine(s, "testname", AString("testval")); // for debug
    
    if (item.LinkName.Len() > kNameSize_Max)
      AddPaxLine(s, "linkpath", item.LinkName);
    
    const UInt64 kPaxSize_Limit = ((UInt64)1 << 33);
    // const UInt64 kPaxSize_Limit = ((UInt64)1 << 1); // for debug
    // bool zero_PackSize = false;
    if (item.PackSize >= kPaxSize_Limit)
    {
      /* GNU TAR in pax mode sets PackSize = 0 in main record, if pack_size >= 8 GiB
         But old 7-Zip doesn't detect "size" property from pax header.
         So we write real size (>= 8 GiB) to main record in binary format,
         and old 7-Zip can decode size correctly */
      // zero_PackSize = true;
      AString v;
      v.Add_UInt64(item.PackSize);
      AddPaxLine(s, "size", v);
    }

    /* GNU TAR encoder can set "devmajor" / "devminor" attributes,
       but GNU TAR decoder doesn't parse "devmajor" / "devminor" */
    if (item.DeviceMajor_Defined)
      AddPax_UInt32_ifBig(s, "devmajor", item.DeviceMajor);
    if (item.DeviceMinor_Defined)
      AddPax_UInt32_ifBig(s, "devminor", item.DeviceMinor);

    AddPax_UInt32_ifBig(s, "uid", item.UID);
    AddPax_UInt32_ifBig(s, "gid", item.GID);
    
    const UInt64 kPax_MTime_Limit = ((UInt64)1 << 33);
    const bool zero_MTime = (
        item.MTime < 0 ||
        item.MTime >= (Int64)kPax_MTime_Limit);

    const CPaxTime &mtime = item.PaxTimes.MTime;
    if (mtime.IsDefined())
    {
      bool needPax = false;
      if (zero_MTime)
        needPax = true;
      else if (TimeOptions.NumDigitsMax > 0)
        if (mtime.Ns != 0 ||
            (mtime.NumDigits != 0 &&
            TimeOptions.RemoveZeroMode == k_PaxTimeMode_DontRemoveZero))
          needPax = true;
      if (needPax)
        AddPaxTime(s, "mtime", mtime, TimeOptions);
    }
    
    if (item.PaxTimes.ATime.IsDefined())
      AddPaxTime(s, "atime", item.PaxTimes.ATime, TimeOptions);
    if (item.PaxTimes.CTime.IsDefined())
      AddPaxTime(s, "ctime", item.PaxTimes.CTime, TimeOptions);

    if (item.User.Len() > kUserNameSize)
      AddPaxLine(s, "uname", item.User);
    if (item.Group.Len() > kGroupNameSize)
      AddPaxLine(s, "gname", item.Group);

    /*
    // for debug
    AString a ("11"); for (int y = 0; y < (1 << 24); y++) AddPaxLine(s, "temp", a);
    */

    const unsigned paxSize = s.Len();
    if (paxSize != 0)
    {
      CItem mi = item;
      mi.LinkName.Empty();
      // SparseBlocks will be ignored by Is_Sparse()
      // mi.SparseBlocks.Clear();
      //  we use "PaxHeader/*" for compatibility with previous 7-Zip decoder

      // GNU TAR writes empty for these fields;
      mi.User.Empty();
      mi.Group.Empty();
      mi.UID = 0;
      mi.GID = 0;

      mi.DeviceMajor_Defined = false;
      mi.DeviceMinor_Defined = false;

      mi.Name = "PaxHeader/@PaxHeader";
      mi.Mode = 0644; // octal
      if (zero_MTime)
        mi.MTime = 0;
      mi.LinkFlag = NLinkFlag::kPax;
      // mi.LinkFlag = 'Z'; // for debug
      mi.PackSize = paxSize;
      // for (unsigned y = 0; y < 1; y++) { // for debug
      RINOK(WriteHeaderReal(mi, true)) // isPax
      RINOK(Write_Data_And_Residual(s, paxSize))
      // } // for debug
      /*
        we can send (zero_MTime) for compatibility with gnu tar output.
        we can send (zero_MTime = false) for better compatibility with old 7-Zip
      */
      // return WriteHeaderReal(item);
      /*
      false, // isPax
      false, // zero_PackSize
      false); // zero_MTime
      */
    }
  }
  else // !PosixMode
  if (!DOES_NAME_FIT_IN_FIELD(item.Name) ||
      !DOES_NAME_FIT_IN_FIELD(item.LinkName))
  {
    // here we can get all fields from main (item) or create new empty item
    /*
    CItem mi;
    mi.SetDefaultWriteFields();
    */
    CItem mi = item;
    mi.LinkName.Empty();
    // SparseBlocks will be ignored by Is_Sparse()
    // mi.SparseBlocks.Clear();
    mi.Name = kLongLink;
    // mi.Name = "././@BAD_LONG_LINK_TEST"; // for debug
    // 21.07 : we set Mode and MTime props as in GNU TAR:
    mi.Mode = 0644; // octal
    mi.MTime = 0;

    mi.User.Empty();
    mi.Group.Empty();
    /*
      gnu tar sets "root" for such items:
        uid_to_uname (0, &uname);
        gid_to_gname (0, &gname);
    */
    /*
    mi.User = "root";
    mi.Group = "root";
    */
    mi.UID = 0;
    mi.GID = 0;
    mi.DeviceMajor_Defined = false;
    mi.DeviceMinor_Defined = false;

    
    for (unsigned i = 0; i < 2; i++)
    {
      const AString *name;
      // We suppose that GNU TAR also writes item for long link before item for LongName?
      if (i == 0)
      {
        mi.LinkFlag = NLinkFlag::kGnu_LongLink;
        name = &item.LinkName;
      }
      else
      {
        mi.LinkFlag = NLinkFlag::kGnu_LongName;
        name = &item.Name;
      }
      if (DOES_NAME_FIT_IN_FIELD(*name))
        continue;
      // GNU TAR writes null character after NAME to file. We do same here:
      const unsigned nameStreamSize = name->Len() + 1;
      mi.PackSize = nameStreamSize;
      // for (unsigned y = 0; y < 3; y++) { // for debug
      RINOK(WriteHeaderReal(mi))
      RINOK(Write_Data_And_Residual(name->Ptr(), nameStreamSize))
      // }
      
      // for debug
      /*
      const unsigned kSize = (1 << 29) + 16;
      CByteBuffer buf;
      buf.Alloc(kSize);
      memset(buf, 0, kSize);
      memcpy(buf, name->Ptr(), name->Len());
      const unsigned nameStreamSize = kSize;
      mi.PackSize = nameStreamSize;
      // for (unsigned y = 0; y < 3; y++) { // for debug
      RINOK(WriteHeaderReal(mi));
      RINOK(WriteBytes(buf, nameStreamSize));
      RINOK(FillDataResidual(nameStreamSize));
      */
    }
  }

  // bool fals = false; if (fals) // for debug: for bit-to-bit output compatibility with GNU TAR

  if (!DOES_NAME_FIT_IN_FIELD(item.Name))
  {
    const unsigned nameLen = item.Name.Len() - namePos;
    if (!needPathCut)
      Prefix.SetFrom(item.Name, namePos - 1);
    else
    {
      Glob_Name = K_PREFIX_PATH_CUT "/_pc_";
      
      if (namePos == 0)
        Glob_Name += "root";
      else
      {
        Glob_Name += "crc32/";
        char temp[12];
        ConvertUInt32ToHex8Digits(CrcCalc(item.Name, namePos - 1), temp);
        Glob_Name += temp;
      }
      
      if (!allowPrefix || Glob_Name.Len() + 1 + nameLen <= kNameSize_Max)
        Glob_Name.Add_Slash();
      else
      {
        Prefix = Glob_Name;
        Glob_Name.Empty();
      }
    }
    Glob_Name.AddFrom(item.Name.Ptr(namePos), nameLen);
  }

  return WriteHeaderReal(item);
}


HRESULT COutArchive::Write_Data(const void *data, unsigned size)
{
  Pos += size;
  return WriteStream(Stream, data, size);
}

HRESULT COutArchive::Write_AfterDataResidual(UInt64 dataSize)
{
  const unsigned v = ((unsigned)dataSize & (kRecordSize - 1));
  if (v == 0)
    return S_OK;
  const unsigned rem = kRecordSize - v;
  Byte buf[kRecordSize];
  memset(buf, 0, rem);
  return Write_Data(buf, rem);
}


HRESULT COutArchive::Write_Data_And_Residual(const void *data, unsigned size)
{
  RINOK(Write_Data(data, size))
  return Write_AfterDataResidual(size);
}


HRESULT COutArchive::WriteFinishHeader()
{
  Byte record[kRecordSize];
  memset(record, 0, kRecordSize);

  const unsigned kNumFinishRecords = 2;

  /* GNU TAR by default uses --blocking-factor=20 (512 * 20 = 10 KiB)
     we also can use cluster alignment:
  const unsigned numBlocks = (unsigned)(Pos / kRecordSize) + kNumFinishRecords;
  const unsigned kNumClusterBlocks = (1 << 3); // 8 blocks = 4 KiB
  const unsigned numFinishRecords = kNumFinishRecords + ((kNumClusterBlocks - numBlocks) & (kNumClusterBlocks - 1));
  */
  
  for (unsigned i = 0; i < kNumFinishRecords; i++)
  {
    RINOK(Write_Data(record, kRecordSize))
  }
  return S_OK;
}

}}
