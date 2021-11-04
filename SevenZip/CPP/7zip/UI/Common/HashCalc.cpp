// HashCalc.cpp

#include "StdAfx.h"

#include "../../../../C/Alloc.h"
#include "../../../../C/CpuArch.h"

#include "../../../Common/DynLimBuf.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringToInt.h"

#include "../../Common/FileStreams.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamObjects.h"
#include "../../Common/StreamUtils.h"

#include "../../Archive/Common/ItemNameUtils.h"

#include "EnumDirItems.h"
#include "HashCalc.h"

using namespace NWindows;

#ifdef EXTERNAL_CODECS
extern const CExternalCodecs *g_ExternalCodecs_Ptr;
#endif

class CHashMidBuf
{
  void *_data;
public:
  CHashMidBuf(): _data(NULL) {}
  operator void *() { return _data; }
  bool Alloc(size_t size)
  {
    if (_data)
      return false;
    _data = ::MidAlloc(size);
    return _data != NULL;
  }
  ~CHashMidBuf() { ::MidFree(_data); }
};

static const char * const k_DefaultHashMethod = "CRC32";

HRESULT CHashBundle::SetMethods(DECL_EXTERNAL_CODECS_LOC_VARS const UStringVector &hashMethods)
{
  UStringVector names = hashMethods;
  if (names.IsEmpty())
    names.Add(UString(k_DefaultHashMethod));

  CRecordVector<CMethodId> ids;
  CObjectVector<COneMethodInfo> methods;
  
  unsigned i;
  for (i = 0; i < names.Size(); i++)
  {
    COneMethodInfo m;
    RINOK(m.ParseMethodFromString(names[i]));

    if (m.MethodName.IsEmpty())
      m.MethodName = k_DefaultHashMethod;
    
    if (m.MethodName == "*")
    {
      CRecordVector<CMethodId> tempMethods;
      GetHashMethods(EXTERNAL_CODECS_LOC_VARS tempMethods);
      methods.Clear();
      ids.Clear();
      FOR_VECTOR (t, tempMethods)
      {
        unsigned index = ids.AddToUniqueSorted(tempMethods[t]);
        if (ids.Size() != methods.Size())
          methods.Insert(index, m);
      }
      break;
    }
    else
    {
      // m.MethodName.RemoveChar(L'-');
      CMethodId id;
      if (!FindHashMethod(EXTERNAL_CODECS_LOC_VARS m.MethodName, id))
        return E_NOTIMPL;
      unsigned index = ids.AddToUniqueSorted(id);
      if (ids.Size() != methods.Size())
        methods.Insert(index, m);
    }
  }

  for (i = 0; i < ids.Size(); i++)
  {
    CMyComPtr<IHasher> hasher;
    AString name;
    RINOK(CreateHasher(EXTERNAL_CODECS_LOC_VARS ids[i], name, hasher));
    if (!hasher)
      throw "Can't create hasher";
    const COneMethodInfo &m = methods[i];
    {
      CMyComPtr<ICompressSetCoderProperties> scp;
      hasher.QueryInterface(IID_ICompressSetCoderProperties, &scp);
      if (scp)
        RINOK(m.SetCoderProps(scp, NULL));
    }
    const UInt32 digestSize = hasher->GetDigestSize();
    if (digestSize > k_HashCalc_DigestSize_Max)
      return E_NOTIMPL;
    CHasherState &h = Hashers.AddNew();
    h.DigestSize = digestSize;
    h.Hasher = hasher;
    h.Name = name;
    for (unsigned k = 0; k < k_HashCalc_NumGroups; k++)
      h.InitDigestGroup(k);
  }

  return S_OK;
}

void CHashBundle::InitForNewFile()
{
  CurSize = 0;
  FOR_VECTOR (i, Hashers)
  {
    CHasherState &h = Hashers[i];
    h.Hasher->Init();
    h.InitDigestGroup(k_HashCalc_Index_Current);
  }
}

void CHashBundle::Update(const void *data, UInt32 size)
{
  CurSize += size;
  FOR_VECTOR (i, Hashers)
    Hashers[i].Hasher->Update(data, size);
}

void CHashBundle::SetSize(UInt64 size)
{
  CurSize = size;
}

static void AddDigests(Byte *dest, const Byte *src, UInt32 size)
{
  unsigned next = 0;
  /*
  // we could use big-endian addition for sha-1 and sha-256
  // but another hashers are little-endian
  if (size > 8)
  {
    for (unsigned i = size; i != 0;)
    {
      i--;
      next += (unsigned)dest[i] + (unsigned)src[i];
      dest[i] = (Byte)next;
      next >>= 8;
    }
  }
  else
  */
  {
    for (unsigned i = 0; i < size; i++)
    {
      next += (unsigned)dest[i] + (unsigned)src[i];
      dest[i] = (Byte)next;
      next >>= 8;
    }
  }
  
  // we use little-endian to store extra bytes
  dest += k_HashCalc_DigestSize_Max;
  for (unsigned i = 0; i < k_HashCalc_ExtraSize; i++)
  {
    next += (unsigned)dest[i];
    dest[i] = (Byte)next;
    next >>= 8;
  }
}

void CHasherState::AddDigest(unsigned groupIndex, const Byte *data)
{
  NumSums[groupIndex]++;
  AddDigests(Digests[groupIndex], data, DigestSize);
}

void CHashBundle::Final(bool isDir, bool isAltStream, const UString &path)
{
  if (isDir)
    NumDirs++;
  else if (isAltStream)
  {
    NumAltStreams++;
    AltStreamsSize += CurSize;
  }
  else
  {
    NumFiles++;
    FilesSize += CurSize;
  }

  Byte pre[16];
  memset(pre, 0, sizeof(pre));
  if (isDir)
    pre[0] = 1;
  
  FOR_VECTOR (i, Hashers)
  {
    CHasherState &h = Hashers[i];
    if (!isDir)
    {
      h.Hasher->Final(h.Digests[0]); // k_HashCalc_Index_Current
      if (!isAltStream)
        h.AddDigest(k_HashCalc_Index_DataSum, h.Digests[0]);
    }

    h.Hasher->Init();
    h.Hasher->Update(pre, sizeof(pre));
    h.Hasher->Update(h.Digests[0], h.DigestSize);
    
    for (unsigned k = 0; k < path.Len(); k++)
    {
      wchar_t c = path[k];
      
      // 21.04: we want same hash for linux and windows paths
      #if CHAR_PATH_SEPARATOR != '/'
      if (c == CHAR_PATH_SEPARATOR)
        c = '/';
      // if (c == (wchar_t)('\\' + 0xf000)) c = '\\'; // to debug WSL
      // if (c > 0xf000 && c < 0xf080) c -= 0xf000; // to debug WSL
      #endif

      Byte temp[2] = { (Byte)(c & 0xFF), (Byte)((c >> 8) & 0xFF) };
      h.Hasher->Update(temp, 2);
    }
  
    Byte tempDigest[k_HashCalc_DigestSize_Max];
    h.Hasher->Final(tempDigest);
    if (!isAltStream)
      h.AddDigest(k_HashCalc_Index_NamesSum, tempDigest);
    h.AddDigest(k_HashCalc_Index_StreamsSum, tempDigest);
  }
}


static void CSum_Name_OriginalToEscape(const AString &src, AString &dest)
{
  dest.Empty();
  for (unsigned i = 0; i < src.Len();)
  {
    char c = src[i++];
    if (c == '\n')
    {
      dest += '\\';
      c = 'n';
    }
    else if (c == '\\')
      dest += '\\';
    dest += c;
  }
}


static bool CSum_Name_EscapeToOriginal(const char *s, AString &dest)
{
  bool isOK = true;
  dest.Empty();
  for (;;)
  {
    char c = *s++;
    if (c == 0)
      break;
    if (c == '\\')
    {
      const char c1 = *s;
      if (c1 == 'n')
      {
        c = '\n';
        s++;
      }
      else if (c1 == '\\')
      {
        c = c1;
        s++;
      }
      else
      {
        // original md5sum returns NULL for such bad strings
        isOK = false;
      }
    }
    dest += c;
  }
  return isOK;
}



static void SetSpacesAndNul(char *s, unsigned num)
{
  for (unsigned i = 0; i < num; i++)
    s[i] = ' ';
  s[num] = 0;
}

static const unsigned kHashColumnWidth_Min = 4 * 2;

static unsigned GetColumnWidth(unsigned digestSize)
{
  const unsigned width = digestSize * 2;
  return width < kHashColumnWidth_Min ? kHashColumnWidth_Min: width;
}


void HashHexToString(char *dest, const Byte *data, UInt32 size);

static void AddHashResultLine(
    AString &_s,
    // bool showHash,
    // UInt64 fileSize, bool showSize,
    const CObjectVector<CHasherState> &hashers
    // unsigned digestIndex, = k_HashCalc_Index_Current
    )
{
  FOR_VECTOR (i, hashers)
  {
    const CHasherState &h = hashers[i];
    char s[k_HashCalc_DigestSize_Max * 2 + 64];
    s[0] = 0;
    // if (showHash)
      HashHexToString(s, h.Digests[k_HashCalc_Index_Current], h.DigestSize);
    const unsigned pos = (unsigned)strlen(s);
    const int numSpaces = (int)GetColumnWidth(h.DigestSize) - (int)pos;
    if (numSpaces > 0)
      SetSpacesAndNul(s + pos, (unsigned)numSpaces);
    if (i != 0)
      _s += ' ';
    _s += s;
  }
  
  /*
  if (showSize)
  {
    _s += ' ';
    static const unsigned kSizeField_Len = 13; // same as in HashCon.cpp
    char s[kSizeField_Len + 32];
    char *p = s;
    SetSpacesAndNul(s, kSizeField_Len);
    p = s + kSizeField_Len;
    ConvertUInt64ToString(fileSize, p);
    int numSpaces = (int)kSizeField_Len - (int)strlen(p);
    if (numSpaces > 0)
      p -= (unsigned)numSpaces;
    _s += p;
  }
  */
}


static void Add_LF(CDynLimBuf &hashFileString, const CHashOptionsLocal &options)
{
  hashFileString += (char)(options.HashMode_Zero.Val ? 0 : '\n');
}




static void WriteLine(CDynLimBuf &hashFileString,
    const CHashOptionsLocal &options,
    const UString &path2,
    bool isDir,
    const AString &methodName,
    const AString &hashesString)
{
  if (options.HashMode_OnlyHash.Val)
  {
    hashFileString += hashesString;
    Add_LF(hashFileString, options);
    return;
  }
     
  UString path = path2;
      
  bool isBin = false;
  const bool zeroMode = options.HashMode_Zero.Val;
  const bool tagMode = options.HashMode_Tag.Val;
  
#if CHAR_PATH_SEPARATOR != '/'
  path.Replace(WCHAR_PATH_SEPARATOR, L'/');
  // path.Replace((wchar_t)('\\' + 0xf000), L'\\'); // to debug WSL
#endif
  
  AString utf8;
  ConvertUnicodeToUTF8(path, utf8);
  
  AString esc;
  CSum_Name_OriginalToEscape(utf8, esc);
  
  if (!zeroMode)
  {
    if (esc != utf8)
    {
      /* Original md5sum writes escape in that case.
      We do same for compatibility with original md5sum. */
      hashFileString += '\\';
    }
  }
  
  if (isDir && !esc.IsEmpty() && esc.Back() != '/')
    esc += '/';
  
  if (tagMode)
  {
    if (!methodName.IsEmpty())
    {
      hashFileString += methodName;
      hashFileString += ' ';
    }
    hashFileString += '(';
    hashFileString += esc;
    hashFileString += ')';
    hashFileString += " = ";
  }
  
  hashFileString += hashesString;
  
  if (!tagMode)
  {
    hashFileString += ' ';
    hashFileString += (char)(isBin ? '*' : ' ');
    hashFileString += esc;
  }

  Add_LF(hashFileString, options);
}



static void WriteLine(CDynLimBuf &hashFileString,
    const CHashOptionsLocal &options,
    const UString &path,
    bool isDir,
    const CHashBundle &hb)
{
  AString methodName;
  if (!hb.Hashers.IsEmpty())
    methodName = hb.Hashers[0].Name;
  
  AString hashesString;
  AddHashResultLine(hashesString, hb.Hashers);
  WriteLine(hashFileString, options, path, isDir, methodName, hashesString);
}


HRESULT HashCalc(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const NWildcard::CCensor &censor,
    const CHashOptions &options,
    AString &errorInfo,
    IHashCallbackUI *callback)
{
  CDirItems dirItems;
  dirItems.Callback = callback;

  if (options.StdInMode)
  {
    CDirItem di;
    di.Size = (UInt64)(Int64)-1;
    di.Attrib = 0;
    di.MTime.dwLowDateTime = 0;
    di.MTime.dwHighDateTime = 0;
    di.CTime = di.ATime = di.MTime;
    dirItems.Items.Add(di);
  }
  else
  {
    RINOK(callback->StartScanning());

    dirItems.SymLinks = options.SymLinks.Val;
    dirItems.ScanAltStreams = options.AltStreamsMode;
    dirItems.ExcludeDirItems = censor.ExcludeDirItems;
    dirItems.ExcludeFileItems = censor.ExcludeFileItems;

    HRESULT res = EnumerateItems(censor,
        options.PathMode,
        UString(),
        dirItems);
    
    if (res != S_OK)
    {
      if (res != E_ABORT)
        errorInfo = "Scanning error";
      return res;
    }
    RINOK(callback->FinishScanning(dirItems.Stat));
  }

  unsigned i;
  CHashBundle hb;
  RINOK(hb.SetMethods(EXTERNAL_CODECS_LOC_VARS options.Methods));
  // hb.Init();

  hb.NumErrors = dirItems.Stat.NumErrors;
  
  if (options.StdInMode)
  {
    RINOK(callback->SetNumFiles(1));
  }
  else
  {
    RINOK(callback->SetTotal(dirItems.Stat.GetTotalBytes()));
  }

  const UInt32 kBufSize = 1 << 15;
  CHashMidBuf buf;
  if (!buf.Alloc(kBufSize))
    return E_OUTOFMEMORY;

  UInt64 completeValue = 0;

  RINOK(callback->BeforeFirstFile(hb));

  /*
  CDynLimBuf hashFileString((size_t)1 << 31);
  const bool needGenerate = !options.HashFilePath.IsEmpty();
  */

  for (i = 0; i < dirItems.Items.Size(); i++)
  {
    CMyComPtr<ISequentialInStream> inStream;
    UString path;
    bool isDir = false;
    bool isAltStream = false;
    
    if (options.StdInMode)
    {
      inStream = new CStdInFileStream;
    }
    else
    {
      path = dirItems.GetLogPath(i);
      const CDirItem &di = dirItems.Items[i];
      isAltStream = di.IsAltStream;

      #ifndef UNDER_CE
      // if (di.AreReparseData())
      if (di.ReparseData.Size() != 0)
      {
        CBufInStream *inStreamSpec = new CBufInStream();
        inStream = inStreamSpec;
        inStreamSpec->Init(di.ReparseData, di.ReparseData.Size());
      }
      else
      #endif
      {
        CInFileStream *inStreamSpec = new CInFileStream;
        inStreamSpec->File.PreserveATime = options.PreserveATime;
        inStream = inStreamSpec;
        isDir = di.IsDir();
        if (!isDir)
        {
          const FString phyPath = dirItems.GetPhyPath(i);
          if (!inStreamSpec->OpenShared(phyPath, options.OpenShareForWrite))
          {
            HRESULT res = callback->OpenFileError(phyPath, ::GetLastError());
            hb.NumErrors++;
            if (res != S_FALSE)
              return res;
            continue;
          }
        }
      }
    }
    
    RINOK(callback->GetStream(path, isDir));
    UInt64 fileSize = 0;

    hb.InitForNewFile();
    
    if (!isDir)
    {
      for (UInt32 step = 0;; step++)
      {
        if ((step & 0xFF) == 0)
        {
          RINOK(callback->SetCompleted(&completeValue));
        }
        UInt32 size;
        RINOK(inStream->Read(buf, kBufSize, &size));
        if (size == 0)
          break;
        hb.Update(buf, size);
        fileSize += size;
        completeValue += size;
      }
    }
    
    hb.Final(isDir, isAltStream, path);
    
    /*
    if (needGenerate
        && (options.HashMode_Dirs.Val || !isDir))
    {
      WriteLine(hashFileString,
          options,
          path, // change it
          isDir,
          hb);
        
      if (hashFileString.IsError())
        return E_OUTOFMEMORY;
    }
    */

    RINOK(callback->SetOperationResult(fileSize, hb, !isDir));
    RINOK(callback->SetCompleted(&completeValue));
  }
  
  /*
  if (needGenerate)
  {
    NFile::NIO::COutFile file;
    if (!file.Create(us2fs(options.HashFilePath), true)) // createAlways
      return GetLastError_noZero_HRESULT();
    if (!file.WriteFull(hashFileString, hashFileString.Len()))
      return GetLastError_noZero_HRESULT();
  }
  */

  return callback->AfterLastFile(hb);
}


static inline char GetHex_Upper(unsigned v)
{
  return (char)((v < 10) ? ('0' + v) : ('A' + (v - 10)));
}

static inline char GetHex_Lower(unsigned v)
{
  return (char)((v < 10) ? ('0' + v) : ('a' + (v - 10)));
}

void HashHexToString(char *dest, const Byte *data, UInt32 size)
{
  dest[size * 2] = 0;
  
  if (!data)
  {
    for (UInt32 i = 0; i < size; i++)
    {
      dest[0] = ' ';
      dest[1] = ' ';
      dest += 2;
    }
    return;
  }
  
  if (size <= 8)
  {
    dest += size * 2;
    for (UInt32 i = 0; i < size; i++)
    {
      const unsigned b = data[i];
      dest -= 2;
      dest[0] = GetHex_Upper((b >> 4) & 0xF);
      dest[1] = GetHex_Upper(b & 0xF);
    }
  }
  else
  {
    for (UInt32 i = 0; i < size; i++)
    {
      const unsigned b = data[i];
      dest[0] = GetHex_Lower((b >> 4) & 0xF);
      dest[1] = GetHex_Lower(b & 0xF);
      dest += 2;
    }
  }
}

void CHasherState::WriteToString(unsigned digestIndex, char *s) const
{
  HashHexToString(s, Digests[digestIndex], DigestSize);

  if (digestIndex != 0 && NumSums[digestIndex] != 1)
  {
    unsigned numExtraBytes = GetNumExtraBytes_for_Group(digestIndex);
    if (numExtraBytes > 4)
      numExtraBytes = 8;
    else // if (numExtraBytes >= 0)
      numExtraBytes = 4;
    // if (numExtraBytes != 0)
    {
      s += strlen(s);
      *s++ = '-';
      // *s = 0;
      HashHexToString(s, GetExtraData_for_Group(digestIndex), numExtraBytes);
    }
  }
}



// ---------- Hash Handler ----------

namespace NHash {

static size_t ParseHexString(const char *s, Byte *dest) throw()
{
  size_t num;
  for (num = 0;; num++, s += 2)
  {
    unsigned c = (Byte)s[0];
    unsigned v0;
         if (c >= '0' && c <= '9') v0 =      (c - '0');
    else if (c >= 'A' && c <= 'F') v0 = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'f') v0 = 10 + (c - 'a');
    else
      return num;
    c = (Byte)s[1];
    unsigned v1;
         if (c >= '0' && c <= '9') v1 =      (c - '0');
    else if (c >= 'A' && c <= 'F') v1 = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'f') v1 = 10 + (c - 'a');
    else
      return num;
    if (dest)
      dest[num] = (Byte)(v1 | (v0 << 4));
  }
}


#define IsWhite(c) ((c) == ' ' || (c) == '\t')

bool CHashPair::IsDir() const
{
  if (Name.IsEmpty() || Name.Back() != '/')
    return false;
  // here we expect that Dir items contain only zeros or no Hash
  for (size_t i = 0; i < Hash.Size(); i++)
    if (Hash[i] != 0)
      return false;
  return true;
}


bool CHashPair::ParseCksum(const char *s)
{
  const char *end;
  
  const UInt32 crc = ConvertStringToUInt32(s, &end);
  if (*end != ' ')
    return false;
  end++;
  
  const UInt64 size = ConvertStringToUInt64(end, &end);
  if (*end != ' ')
    return false;
  end++;
  
  Name = end;
  
  Hash.Alloc(4);
  SetBe32(Hash, crc);

  Size_from_Arc = size;
  Size_from_Arc_Defined = true;

  return true;
}



static const char *SkipWhite(const char *s)
{
  while (IsWhite(*s))
    s++;
  return s;
}

static const char * const k_CsumMethodNames[] =
{
    "sha256"
  , "sha224"
//  , "sha512/224"
//  , "sha512/256"
  , "sha512"
  , "sha384"
  , "sha1"
  , "md5"
  , "blake2b"
  , "crc64"
  , "crc32"
  , "cksum"
};

static UString GetMethod_from_FileName(const UString &name)
{
  AString s;
  ConvertUnicodeToUTF8(name, s);
  const int dotPos = s.ReverseFind_Dot();
  const char *src = s.Ptr();
  bool isExtension = false;
  if (dotPos >= 0)
  {
    isExtension = true;
    src = s.Ptr(dotPos + 1);
  }
  const char *m = "";
  unsigned i;
  for (i = 0; i < ARRAY_SIZE(k_CsumMethodNames); i++)
  {
    m = k_CsumMethodNames[i];
    if (isExtension)
    {
      if (StringsAreEqual_Ascii(src, m))
        break;
    }
    else if (IsString1PrefixedByString2_NoCase_Ascii(src, m))
      if (StringsAreEqual_Ascii(src + strlen(m), "sums"))
        break;
  }
  UString res;
  if (i != ARRAY_SIZE(k_CsumMethodNames))
    res = m;
  return res;
}


bool CHashPair::Parse(const char *s)
{
  // here we keep compatibility with original md5sum / shasum
  bool escape = false;

  s = SkipWhite(s);

  if (*s == '\\')
  {
    s++;
    escape = true;
  }
  
  // const char *kMethod = GetMethod_from_FileName(s);
  // if (kMethod)
  if (ParseHexString(s, NULL) < 4)
  {
    // BSD-style checksum line
    {
      const char *s2 = s;
      for (; *s2 != 0; s2++)
      {
        const char c = *s2;
        if (c == 0)
          return false;
        if (c == ' ' || c == '(')
          break;
      }
      Method.SetFrom(s, (unsigned)(s2 - s));
      s = s2;
    }
    IsBSD = true;
    if (*s == ' ')
      s++;
    if (*s != '(')
      return false;
    s++;
    {
      const char *s2 = s;
      for (; *s2 != 0; s2++)
      {}
      for (;;)
      {
        s2--;
        if (s2 < s)
          return false;
        if (*s2 == ')')
          break;
      }
      Name.SetFrom(s, (unsigned)(s2 - s));
      s = s2 + 1;
    }

    s = SkipWhite(s);
    if (*s != '=')
      return false;
    s++;
    s = SkipWhite(s);
  }

  {
    const size_t num = ParseHexString(s, NULL);
    Hash.Alloc(num);
    ParseHexString(s, Hash);
    const size_t numChars = num * 2;
    HashString.SetFrom(s, (unsigned)numChars);
    s += numChars;
  }
  
  if (IsBSD)
  {
    if (*s != 0)
      return false;
    if (escape)
    {
      AString temp = Name;
      return CSum_Name_EscapeToOriginal(temp, Name);
    }
    return true;
  }

  if (*s == 0)
    return true;

  if (*s != ' ')
    return false;
  s++;
  const char c = *s;
  if (c != ' '
      && c != '*'
      && c != 'U' // shasum Universal
      && c != '^' // shasum 0/1
     )
    return false;
  Mode = c;
  s++;
  if (escape)
    return CSum_Name_EscapeToOriginal(s, Name);
  Name = s;
  return true;
}


static bool GetLine(CByteBuffer &buf, bool zeroMode, bool cr_lf_Mode, size_t &posCur, AString &s)
{
  s.Empty();
  size_t pos = posCur;
  const Byte *p = buf;
  unsigned numDigits = 0;
  for (; pos < buf.Size(); pos++)
  {
    const Byte b = p[pos];
    if (b == 0)
    {
      numDigits = 1;
      break;
    }
    if (zeroMode)
      continue;
    if (b == 0x0a)
    {
      numDigits = 1;
      break;
    }
    if (!cr_lf_Mode)
      continue;
    if (b == 0x0d)
    {
      if (pos + 1 >= buf.Size())
      {
        numDigits = 1;
        break;
        // return false;
      }
      if (p[pos + 1] == 0x0a)
      {
        numDigits = 2;
        break;
      }
    }
  }
  s.SetFrom((const char *)(p + posCur), (unsigned)(pos - posCur));
  posCur = pos + numDigits;
  return true;
}


static bool Is_CR_LF_Data(const Byte *buf, size_t size)
{
  bool isCrLf = false;
  for (size_t i = 0; i < size;)
  {
    const Byte b = buf[i];
    if (b == 0x0a)
      return false;
    if (b == 0x0d)
    {
      if (i == size - 1)
        return false;
      if (buf[i + 1] != 0x0a)
        return false;
      isCrLf = true;
      i += 2;
    }
    else
      i++;
  }
  return isCrLf;
}


static const Byte kArcProps[] =
{
  // kpidComment,
  kpidCharacts
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidMethod
};

static const Byte kRawProps[] =
{
  kpidChecksum
};


STDMETHODIMP CHandler::GetParent(UInt32 /* index */ , UInt32 *parent, UInt32 *parentType)
{
  *parentType = NParentType::kDir;
  *parent = (UInt32)(Int32)-1;
  return S_OK;
}

STDMETHODIMP CHandler::GetNumRawProps(UInt32 *numProps)
{
  *numProps = ARRAY_SIZE(kRawProps);
  return S_OK;
}

STDMETHODIMP CHandler::GetRawPropInfo(UInt32 index, BSTR *name, PROPID *propID)
{
  *propID = kRawProps[index];
  *name = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType)
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  if (propID == kpidChecksum)
  {
    const CHashPair &hp = HashPairs[index];
    if (hp.Hash.Size() > 0)
    {
      *data = hp.Hash;
      *dataSize = (UInt32)hp.Hash.Size();
      *propType = NPropDataType::kRaw;
    }
    return S_OK;
  }

  return S_OK;
}

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = HashPairs.Size();
  return S_OK;
}

static void Add_OptSpace_String(UString &dest, const char *src)
{
  dest.Add_Space_if_NotEmpty();
  dest += src;
}

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    /*
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      // if (_sres == k_Base64_RES_NeedMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (v != 0)
        prop = v;
      break;
    }
    */
    case kpidCharacts:
    {
      UString s;
      if (_hashSize_Defined)
      {
        s.Add_Space_if_NotEmpty();
        s.Add_UInt32(_hashSize * 8);
        s += "-bit";
      }
      if (!_nameExtenstion.IsEmpty())
      {
        s.Add_Space_if_NotEmpty();
        s += _nameExtenstion;
      }
      if (_is_PgpMethod)
      {
        Add_OptSpace_String(s, "PGP");
        if (!_pgpMethod.IsEmpty())
        {
          s += ":";
          s += _pgpMethod;
        }
      }
      if (_is_ZeroMode)
        Add_OptSpace_String(s, "ZERO");
      if (_are_there_Tags)
        Add_OptSpace_String(s, "TAG");
      if (_are_there_Dirs)
        Add_OptSpace_String(s, "DIRS");
      prop = s;
      break;
    }

    case kpidReadOnly:
    {
      if (_isArc)
        if (!CanUpdate())
          prop = true;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}


STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  // COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  CHashPair &hp = HashPairs[index];
  switch (propID)
  {
    case kpidIsDir:
    {
      prop = hp.IsDir();
      break;
    }
    case kpidPath:
    {
      UString path;
      hp.Get_UString_Path(path);

      NArchive::NItemName::ReplaceToOsSlashes_Remove_TailSlash(path,
          true); // useBackslashReplacement

      prop = path;
      break;
    }
    case kpidSize:
    {
      // client needs processed size of last file
      if (hp.Size_from_Disk_Defined)
        prop = (UInt64)hp.Size_from_Disk;
      else if (hp.Size_from_Arc_Defined)
        prop = (UInt64)hp.Size_from_Arc;
      break;
    }
    case kpidPackSize:
    {
      prop = (UInt64)hp.Hash.Size();
      break;
    }
    case kpidMethod:
    {
      if (!hp.Method.IsEmpty())
        prop = hp.Method;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  // COM_TRY_END
}


static HRESULT ReadStream_to_Buf(IInStream *stream, CByteBuffer &buf, IArchiveOpenCallback *openCallback)
{
  buf.Free();
  UInt64 len;
  RINOK(stream->Seek(0, STREAM_SEEK_END, &len));
  if (len == 0 || len >= ((UInt64)1 << 31))
    return S_FALSE;
  RINOK(stream->Seek(0, STREAM_SEEK_SET, NULL));
  buf.Alloc((size_t)len);
  UInt64 pos = 0;
  // return ReadStream_FALSE(stream, buf, (size_t)len);
  for (;;)
  {
    const UInt32 kBlockSize = ((UInt32)1 << 24);
    const UInt32 curSize = (len < kBlockSize) ? (UInt32)len : kBlockSize;
    UInt32 processedSizeLoc;
    RINOK(stream->Read((Byte *)buf + pos, curSize, &processedSizeLoc));
    if (processedSizeLoc == 0)
      return E_FAIL;
    len -= processedSizeLoc;
    pos += processedSizeLoc;
    if (len == 0)
      return S_OK;
    if (openCallback)
    {
      const UInt64 files = 0;
      RINOK(openCallback->SetCompleted(&files, &pos));
    }
  }
}


STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *openCallback)
{
  COM_TRY_BEGIN
  {
    Close();

    CByteBuffer buf;
    RINOK(ReadStream_to_Buf(stream, buf, openCallback))

    CObjectVector<CHashPair> &pairs = HashPairs;

    bool zeroMode = false;
    bool cr_lf_Mode = false;
    {
      for (size_t i = 0; i < buf.Size(); i++)
        if (buf[i] == 0)
        {
          zeroMode = true;
          break;
        }
    }
    _is_ZeroMode = zeroMode;
    if (!zeroMode)
      cr_lf_Mode = Is_CR_LF_Data(buf, buf.Size());

    if (openCallback)
    {
      CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
      openCallback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback);
      NCOM::CPropVariant prop;
      if (openVolumeCallback)
      {
        RINOK(openVolumeCallback->GetProperty(kpidName, &prop));
        if (prop.vt == VT_BSTR)
          _nameExtenstion = GetMethod_from_FileName(prop.bstrVal);
      }
    }

    bool cksumMode = false;
    if (_nameExtenstion.IsEqualTo_Ascii_NoCase("cksum"))
      cksumMode = true;
    _is_CksumMode = cksumMode;

    size_t pos = 0;
    AString s;
    bool minusMode = false;
    unsigned numLines = 0;
    
    while (pos < buf.Size())
    {
      if (!GetLine(buf, zeroMode, cr_lf_Mode, pos, s))
        return S_FALSE;
      numLines++;
      if (s.IsEmpty())
        continue;
      
      if (s.IsPrefixedBy_Ascii_NoCase("; "))
      {
        if (numLines != 1)
          return S_FALSE;
        // comment line of FileVerifier++
        continue;
      }
      
      if (s.IsPrefixedBy_Ascii_NoCase("-----"))
      {
        if (minusMode)
          break; // end of pgp mode
        minusMode = true;
        if (s.IsPrefixedBy_Ascii_NoCase("-----BEGIN PGP SIGNED MESSAGE"))
        {
          if (_is_PgpMethod)
            return S_FALSE;
          if (!GetLine(buf, zeroMode, cr_lf_Mode, pos, s))
            return S_FALSE;
          const char *kStart = "Hash: ";
          if (!s.IsPrefixedBy_Ascii_NoCase(kStart))
            return S_FALSE;
          _pgpMethod = s.Ptr((unsigned)strlen(kStart));
          _is_PgpMethod = true;
        }
        continue;
      }
      
      CHashPair pair;
      pair.FullLine = s;
      if (cksumMode)
      {
        if (!pair.ParseCksum(s))
          return S_FALSE;
      }
      else if (!pair.Parse(s))
        return S_FALSE;
      pairs.Add(pair);
    }

    {
      unsigned hashSize = 0;
      bool hashSize_Dismatch = false;
      for (unsigned i = 0; i < HashPairs.Size(); i++)
      {
        const CHashPair &hp = HashPairs[i];
        if (i == 0)
          hashSize = (unsigned)hp.Hash.Size();
        else
          if (hashSize != hp.Hash.Size())
            hashSize_Dismatch = true;

        if (hp.IsBSD)
          _are_there_Tags = true;
        if (!_are_there_Dirs && hp.IsDir())
          _are_there_Dirs = true;
      }
      if (!hashSize_Dismatch && hashSize != 0)
      {
        _hashSize = hashSize;
        _hashSize_Defined = true;
      }
    }

    _phySize = buf.Size();
    _isArc = true;
    return S_OK;
  }
  COM_TRY_END
}


void CHandler::ClearVars()
{
  _phySize = 0;
  _isArc = false;
  _is_CksumMode = false;
  _is_PgpMethod = false;
  _is_ZeroMode = false;
  _are_there_Tags = false;
  _are_there_Dirs = false;
  _hashSize_Defined = false;
  _hashSize = 0;
}


STDMETHODIMP CHandler::Close()
{
  ClearVars();
  _nameExtenstion.Empty();
  _pgpMethod.Empty();
  HashPairs.Clear();
  return S_OK;
}


static bool CheckDigests(const Byte *a, const Byte *b, size_t size)
{
  if (size <= 8)
  {
    /* we use reversed order for one digest, when text representation
       uses big-order for crc-32 and crc-64 */
    for (size_t i = 0; i < size; i++)
      if (a[i] != b[size - 1 - i])
        return false;
    return true;
  }
  {
    for (size_t i = 0; i < size; i++)
      if (a[i] != b[i])
        return false;
    return true;
  }
}


static void AddDefaultMethod(UStringVector &methods, unsigned size)
{
  const char *m = NULL;
       if (size == 32) m = "sha256";
  else if (size == 20) m = "sha1";
  else if (size == 16) m = "md5";
  else if (size ==  8) m = "crc64";
  else if (size ==  4) m = "crc32";
  else
    return;
  #ifdef EXTERNAL_CODECS
  const CExternalCodecs *__externalCodecs = g_ExternalCodecs_Ptr;
  #endif
  CMethodId id;
  if (FindHashMethod(EXTERNAL_CODECS_LOC_VARS
      AString(m), id))
    methods.Add(UString(m));
}


STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN

  /*
  if (testMode == 0)
    return E_NOTIMPL;
  */

  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = HashPairs.Size();
  if (numItems == 0)
    return S_OK;

  #ifdef EXTERNAL_CODECS
  const CExternalCodecs *__externalCodecs = g_ExternalCodecs_Ptr;
  #endif
  
  CHashBundle hb_Glob;
  // UStringVector methods = options.Methods;
  UStringVector methods;
  
  if (methods.IsEmpty() && !_nameExtenstion.IsEmpty())
  {
    AString utf;
    ConvertUnicodeToUTF8(_nameExtenstion, utf);
    CMethodId id;
    if (FindHashMethod(EXTERNAL_CODECS_LOC_VARS utf, id))
      methods.Add(_nameExtenstion);
  }
  
  if (methods.IsEmpty() && !_pgpMethod.IsEmpty())
  {
    CMethodId id;
    if (FindHashMethod(EXTERNAL_CODECS_LOC_VARS _pgpMethod, id))
      methods.Add(UString(_pgpMethod));
  }

  if (methods.IsEmpty() && _pgpMethod.IsEmpty() && _hashSize_Defined)
    AddDefaultMethod(methods, _hashSize);

  RINOK(hb_Glob.SetMethods(
      EXTERNAL_CODECS_LOC_VARS
      methods));

  CMyComPtr<IArchiveUpdateCallbackFile> updateCallbackFile;
  extractCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&updateCallbackFile);
  if (!updateCallbackFile)
    return E_NOTIMPL;
  {
    CMyComPtr<IArchiveGetDiskProperty> GetDiskProperty;
    extractCallback->QueryInterface(IID_IArchiveGetDiskProperty, (void **)&GetDiskProperty);
    if (GetDiskProperty)
    {
      UInt64 totalSize = 0;
      UInt32 i;
      for (i = 0; i < numItems; i++)
      {
        const UInt32 index = allFilesMode ? i : indices[i];
        const CHashPair &hp = HashPairs[index];
        if (hp.IsDir())
          continue;
        {
          NCOM::CPropVariant prop;
          RINOK(GetDiskProperty->GetDiskProperty(index, kpidSize, &prop));
          if (prop.vt != VT_UI8)
            continue;
          totalSize += prop.uhVal.QuadPart;
        }
      }
      RINOK(extractCallback->SetTotal(totalSize));
      // RINOK(Hash_SetTotalUnpacked->Hash_SetTotalUnpacked(indices, numItems));
    }
  }

  const UInt32 kBufSize = 1 << 15;
  CHashMidBuf buf;
  if (!buf.Alloc(kBufSize))
    return E_OUTOFMEMORY;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);
  lps->InSize = lps->OutSize = 0;

  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    RINOK(lps->SetCur());
    const UInt32 index = allFilesMode ? i : indices[i];

    CHashPair &hp = HashPairs[index];
    
    UString path;
    hp.Get_UString_Path(path);

    CMyComPtr<ISequentialInStream> inStream;
    const bool isDir = hp.IsDir();
    if (!isDir)
    {
      RINOK(updateCallbackFile->GetStream2(index, &inStream, NUpdateNotifyOp::kHashRead));
      if (!inStream)
      {
        continue; // we have shown error in GetStream2()
      }
      // askMode = NArchive::NExtract::NAskMode::kSkip;
    }

    Int32 askMode = testMode ?
        NArchive::NExtract::NAskMode::kTest :
        NArchive::NExtract::NAskMode::kExtract;

    CMyComPtr<ISequentialOutStream> realOutStream;
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    /* PrepareOperation() can expect kExtract to set
       Attrib and security of output file */
    askMode = NArchive::NExtract::NAskMode::kReadExternal;

    extractCallback->PrepareOperation(askMode);
    
    const bool isAltStream = false;

    UInt64 fileSize = 0;

    CHashBundle hb_Loc;
    
    CHashBundle *hb_Use = &hb_Glob;

    HRESULT res_SetMethods = S_OK;

    UStringVector methods_loc;
    
    if (!hp.Method.IsEmpty())
    {
      hb_Use = &hb_Loc;
      CMethodId id;
      if (FindHashMethod(EXTERNAL_CODECS_LOC_VARS hp.Method, id))
      {
        methods_loc.Add(UString(hp.Method));
        RINOK(hb_Loc.SetMethods(
            EXTERNAL_CODECS_LOC_VARS
            methods_loc));
      }
      else
        res_SetMethods = E_NOTIMPL;
    }
    else if (methods.IsEmpty())
    {
      AddDefaultMethod(methods_loc, (unsigned)hp.Hash.Size());
      if (!methods_loc.IsEmpty())
      {
        hb_Use = &hb_Loc;
        RINOK(hb_Loc.SetMethods(
            EXTERNAL_CODECS_LOC_VARS
            methods_loc));
      }
    }

    const bool isSupportedMode = hp.IsSupportedMode();
    hb_Use->InitForNewFile();
    
    if (inStream)
    {
      for (UInt32 step = 0;; step++)
      {
        if ((step & 0xFF) == 0)
        {
          RINOK(lps->SetRatioInfo(NULL, &fileSize));
        }
        UInt32 size;
        RINOK(inStream->Read(buf, kBufSize, &size));
        if (size == 0)
          break;
        hb_Use->Update(buf, size);
        if (realOutStream)
        {
          RINOK(WriteStream(realOutStream, buf, size));
        }
        fileSize += size;
      }

      hp.Size_from_Disk = fileSize;
      hp.Size_from_Disk_Defined = true;
    }

    realOutStream.Release();
    inStream.Release();

    lps->InSize += hp.Hash.Size();
    lps->OutSize += fileSize;

    hb_Use->Final(isDir, isAltStream, path);

    Int32 opRes = NArchive::NExtract::NOperationResult::kUnsupportedMethod;
    if (isSupportedMode
        && res_SetMethods != E_NOTIMPL
        && hb_Use->Hashers.Size() > 0
        )
    {
      const CHasherState &hs = hb_Use->Hashers[0];
      if (hs.DigestSize == hp.Hash.Size())
      {
        opRes = NArchive::NExtract::NOperationResult::kCRCError;
        if (CheckDigests(hp.Hash, hs.Digests[0], hs.DigestSize))
          if (!hp.Size_from_Arc_Defined || hp.Size_from_Arc == fileSize)
            opRes = NArchive::NExtract::NOperationResult::kOK;
      }
    }

    RINOK(extractCallback->SetOperationResult(opRes));
  }

  return lps->SetCur();

  COM_TRY_END
}


// ---------- UPDATE ----------

struct CUpdateItem
{
  int IndexInArc;
  unsigned IndexInClient;
  UInt64 Size;
  bool NewData;
  bool NewProps;
  bool IsDir;
  UString Path;

  CUpdateItem(): Size(0), IsDir(false) {}
};


static HRESULT GetPropString(IArchiveUpdateCallback *callback, UInt32 index, PROPID propId,
    UString &res,
    bool convertSlash)
{
  NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(index, propId, &prop));
  if (prop.vt == VT_BSTR)
  {
    res = prop.bstrVal;
    if (convertSlash)
      NArchive::NItemName::ReplaceSlashes_OsToUnix(res);
  }
  else if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;
  return S_OK;
}


STDMETHODIMP CHandler::GetFileTimeType(UInt32 *type)
{
  *type = NFileTimeType::kUnix;
  return S_OK;
}


STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *callback)
{
  COM_TRY_BEGIN

  if (_isArc && !CanUpdate())
    return E_NOTIMPL;

  // const UINT codePage = CP_UTF8; // // (_forceCodePage ? _specifiedCodePage : _openCodePage);
  // const unsigned utfFlags = g_Unicode_To_UTF8_Flags;
  CObjectVector<CUpdateItem> updateItems;

  UInt64 complexity = 0;

  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    CUpdateItem ui;
    Int32 newData;
    Int32 newProps;
    UInt32 indexInArc;
    
    if (!callback)
      return E_FAIL;
    
    RINOK(callback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArc));

    ui.NewProps = IntToBool(newProps);
    ui.NewData = IntToBool(newData);
    ui.IndexInArc = (int)indexInArc;
    ui.IndexInClient = i;
    if (IntToBool(newProps))
    {
      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidIsDir, &prop));
        if (prop.vt == VT_EMPTY)
          ui.IsDir = false;
        else if (prop.vt != VT_BOOL)
          return E_INVALIDARG;
        else
          ui.IsDir = (prop.boolVal != VARIANT_FALSE);
      }

      RINOK(GetPropString(callback, i, kpidPath, ui.Path,
          true)); // convertSlash
      /*
      if (ui.IsDir && !ui.Name.IsEmpty() && ui.Name.Back() != '/')
        ui.Name += '/';
      */
    }

    if (IntToBool(newData))
    {
      NCOM::CPropVariant prop;
      RINOK(callback->GetProperty(i, kpidSize, &prop));
      if (prop.vt == VT_UI8)
      {
        ui.Size = prop.uhVal.QuadPart;
        complexity += ui.Size;
      }
      else if (prop.vt == VT_EMPTY)
        ui.Size = (UInt64)(Int64)-1;
      else
        return E_INVALIDARG;
    }
    
    updateItems.Add(ui);
  }

  if (complexity != 0)
  {
    RINOK(callback->SetTotal(complexity));
  }

  #ifdef EXTERNAL_CODECS
  const CExternalCodecs *__externalCodecs = g_ExternalCodecs_Ptr;
  #endif

  CHashBundle hb;
  UStringVector methods;
  if (!_methods.IsEmpty())
  {
    FOR_VECTOR(k, _methods)
    {
      methods.Add(_methods[k]);
    }
  }
  else if (_crcSize_WasSet)
  {
    AddDefaultMethod(methods, _crcSize);
  }
  else
  {
    CMyComPtr<IArchiveGetRootProps> getRootProps;
    callback->QueryInterface(IID_IArchiveGetRootProps, (void **)&getRootProps);

    NCOM::CPropVariant prop;
    if (getRootProps)
    {
      RINOK(getRootProps->GetRootProp(kpidArcFileName, &prop));
      if (prop.vt == VT_BSTR)
      {
        const UString method = GetMethod_from_FileName(prop.bstrVal);
        if (!method.IsEmpty())
          methods.Add(method);
      }
    }
  }

  RINOK(hb.SetMethods(EXTERNAL_CODECS_LOC_VARS methods));

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(callback, true);

  const UInt32 kBufSize = 1 << 15;
  CHashMidBuf buf;
  if (!buf.Alloc(kBufSize))
    return E_OUTOFMEMORY;

  CDynLimBuf hashFileString((size_t)1 << 31);

  CHashOptionsLocal options = _options;
  
  if (_isArc)
  {
    if (!options.HashMode_Zero.Def && _is_ZeroMode)
      options.HashMode_Zero.Val = true;
    if (!options.HashMode_Tag.Def && _are_there_Tags)
      options.HashMode_Tag.Val = true;
    if (!options.HashMode_Dirs.Def && _are_there_Dirs)
      options.HashMode_Dirs.Val = true;
  }
  if (options.HashMode_OnlyHash.Val && updateItems.Size() != 1)
    options.HashMode_OnlyHash.Val = false;

  lps->OutSize = 0;
  complexity = 0;

  for (i = 0; i < updateItems.Size(); i++)
  {
    lps->InSize = complexity;
    RINOK(lps->SetCur());

    const CUpdateItem &ui = updateItems[i];
    
    /*
    CHashPair item;
    if (!ui.NewProps)
      item = HashPairs[(unsigned)ui.IndexInArc];
    */

    if (ui.NewData)
    {
      UInt64 currentComplexity = ui.Size;
      CMyComPtr<ISequentialInStream> fileInStream;
      bool needWrite = true;
      {
        HRESULT res = callback->GetStream(ui.IndexInClient, &fileInStream);

        if (res == S_FALSE)
          needWrite = false;
        else
        {
          RINOK(res);
          
          if (fileInStream)
          {
            CMyComPtr<IStreamGetProps> getProps;
            fileInStream->QueryInterface(IID_IStreamGetProps, (void **)&getProps);
            if (getProps)
            {
              FILETIME mTime;
              UInt64 size2;
              if (getProps->GetProps(&size2, NULL, NULL, &mTime, NULL) == S_OK)
              {
                currentComplexity = size2;
                // item.MTime = NWindows::NTime::FileTimeToUnixTime64(mTime);;
              }
            }
          }
          else
          {
            currentComplexity = 0;
          }
        }
      }

      hb.InitForNewFile();
      const bool isDir = ui.IsDir;
      
      if (needWrite && fileInStream && !isDir)
      {
        UInt64 fileSize = 0;
        for (UInt32 step = 0;; step++)
        {
          if ((step & 0xFF) == 0)
          {
            RINOK(lps->SetRatioInfo(&fileSize, NULL));
            // RINOK(callback->SetCompleted(&completeValue));
          }
          UInt32 size;
          RINOK(fileInStream->Read(buf, kBufSize, &size));
          if (size == 0)
            break;
          hb.Update(buf, size);
          fileSize += size;
        }
        currentComplexity = fileSize;
      }

      fileInStream.Release();
      const bool isAltStream = false;
      hb.Final(isDir, isAltStream, ui.Path);

      if (options.HashMode_Dirs.Val || !isDir)
      {
        if (!hb.Hashers.IsEmpty())
          lps->OutSize += hb.Hashers[0].DigestSize;
        WriteLine(hashFileString,
            options,
            ui.Path,
            isDir,
            hb);
        if (hashFileString.IsError())
          return E_OUTOFMEMORY;
      }

      complexity += currentComplexity;
      RINOK(callback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
    }
    else
    {
      // old data
      const CHashPair &existItem = HashPairs[(unsigned)ui.IndexInArc];
      if (ui.NewProps)
      {
        WriteLine(hashFileString,
            options,
            ui.Path,
            ui.IsDir,
            existItem.Method, existItem.HashString
            );
      }
      else
      {
        hashFileString += existItem.FullLine;
        Add_LF(hashFileString, options);
      }
    }
    if (hashFileString.IsError())
      return E_OUTOFMEMORY;
  }

  RINOK(WriteStream(outStream, hashFileString, hashFileString.Len()));

  return S_OK;
  COM_TRY_END
}



HRESULT CHandler::SetProperty(const wchar_t *nameSpec, const PROPVARIANT &value)
{
  UString name = nameSpec;
  name.MakeLower_Ascii();
  if (name.IsEmpty())
    return E_INVALIDARG;
  
  if (name.IsEqualTo("m")) // "hm" hash method
  {
    // COneMethodInfo omi;
    // RINOK(omi.ParseMethodFromPROPVARIANT(L"", value));
    // _methods.Add(omi.MethodName); // change it. use omi.PropsString
    if (value.vt != VT_BSTR)
      return E_INVALIDARG;
    UString s (value.bstrVal);
    _methods.Add(s);
    return S_OK;
  }

  if (name.IsEqualTo("flags"))
  {
    if (value.vt != VT_BSTR)
      return E_INVALIDARG;
    if (!_options.ParseString(value.bstrVal))
      return E_INVALIDARG;
    return S_OK;
  }

  if (name.IsPrefixedBy_Ascii_NoCase("crc"))
  {
    name.Delete(0, 3);
    _crcSize = 4;
    _crcSize_WasSet = true;
    return ParsePropToUInt32(name, value, _crcSize);
  }

  // common properties
  if (name.IsPrefixedBy_Ascii_NoCase("mt")
      || name.IsPrefixedBy_Ascii_NoCase("memuse"))
    return S_OK;
  
  return E_INVALIDARG;
}


STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  COM_TRY_BEGIN

  InitProps();

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetProperty(names[i], values[i]));
  }
  return S_OK;
  COM_TRY_END
}

CHandler::CHandler()
{
  ClearVars();
  InitProps();
}

}



static IInArchive  *CreateHashHandler_In()  { return new NHash::CHandler; }
static IOutArchive *CreateHashHandler_Out() { return new NHash::CHandler; }

void Codecs_AddHashArcHandler(CCodecs *codecs)
{
  {
    CArcInfoEx item;
    
    item.Name = "Hash";
    item.CreateInArchive = CreateHashHandler_In;
    item.CreateOutArchive = CreateHashHandler_Out;
    item.IsArcFunc = NULL;
    item.Flags =
        NArcInfoFlags::kKeepName
      | NArcInfoFlags::kStartOpen
      | NArcInfoFlags::kByExtOnlyOpen
      // | NArcInfoFlags::kPureStartOpen
      | NArcInfoFlags::kHashHandler
      ;
  
    // ubuntu uses "SHA256SUMS" file
    item.AddExts(UString (
        "sha256 sha512 sha224 sha384 sha1 sha md5"
        // "b2sum"
        " crc32 crc64"
        " asc"
        " cksum"
        ),
        UString());

    item.UpdateEnabled = (item.CreateOutArchive != NULL);
    item.SignatureOffset = 0;
    // item.Version = MY_VER_MIX;
    item.NewInterface = true;
    
    item.Signatures.AddNew().CopyFrom(NULL, 0);
    
    codecs->Formats.Add(item);
  }
}
