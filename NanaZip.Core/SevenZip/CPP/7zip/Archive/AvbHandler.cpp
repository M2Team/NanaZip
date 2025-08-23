// AvbHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get32(p) GetBe32(p)
#define Get64(p) GetBe64(p)

#define G32(_offs_, dest) dest = Get32(p + (_offs_))
#define G64(_offs_, dest) dest = Get64(p + (_offs_))

using namespace NWindows;

namespace NArchive {

namespace NExt {
API_FUNC_IsArc IsArc_Ext_PhySize(const Byte *p, size_t size, UInt64 *phySize);
}

namespace NAvb {

static void AddNameToString(AString &s, const Byte *name, unsigned size, bool strictConvert)
{
  for (unsigned i = 0; i < size; i++)
  {
    Byte c = name[i];
    if (c == 0)
      return;
    if (strictConvert && c < 32)
      c = '_';
    s += (char)c;
  }
}

/* Maximum size of a vbmeta image - 64 KiB. */
#define VBMETA_MAX_SIZE (64 * 1024)

#define SIGNATURE { 'A', 'V', 'B', 'f', 0, 0, 0, 1 }
  
static const unsigned k_SignatureSize = 8;
static const Byte k_Signature[k_SignatureSize] = SIGNATURE;

// #define AVB_FOOTER_MAGIC "AVBf"
// #define AVB_FOOTER_MAGIC_LEN 4
/* The current footer version used - keep in sync with avbtool. */
#define AVB_FOOTER_VERSION_MAJOR 1

/* The struct used as a footer used on partitions, used to find the
 * AvbVBMetaImageHeader struct. This struct is always stored at the
 * end of a partition.
 */
// #define AVB_FOOTER_SIZE 64
static const unsigned kFooterSize = 64;

struct CFooter
{
  /*   0: Four bytes equal to "AVBf" (AVB_FOOTER_MAGIC). */
  // Byte magic[AVB_FOOTER_MAGIC_LEN];
  /*   4: The major version of the footer struct. */
  UInt32 version_major;
  /*   8: The minor version of the footer struct. */
  UInt32 version_minor;

  /*  12: The original size of the image on the partition. */
  UInt64 original_image_size;

  /*  20: The offset of the |AvbVBMetaImageHeader| struct. */
  UInt64 vbmeta_offset;

  /*  28: The size of the vbmeta block (header + auth + aux blocks). */
  UInt64 vbmeta_size;

  /*  36: Padding to ensure struct is size AVB_FOOTER_SIZE bytes. This
   * must be set to zeroes.
   */
  Byte reserved[28];

  void Parse(const Byte *p)
  {
    G32 (4, version_major);
    G32 (8, version_minor);
    G64 (12, original_image_size);
    G64 (20, vbmeta_offset);
    G64 (28, vbmeta_size);
  }
};


/* Size of the vbmeta image header. */
#define AVB_VBMETA_IMAGE_HEADER_SIZE 256

/* Magic for the vbmeta image header. */
// #define AVB_MAGIC "AVB0"
// #define AVB_MAGIC_LEN 4
/* Maximum size of the release string including the terminating NUL byte. */
#define AVB_RELEASE_STRING_SIZE 48

struct AvbVBMetaImageHeader
{
  /*   0: Four bytes equal to "AVB0" (AVB_MAGIC). */
  // Byte magic[AVB_MAGIC_LEN];

  /*   4: The major version of libavb required for this header. */
  UInt32 required_libavb_version_major;
  /*   8: The minor version of libavb required for this header. */
  UInt32 required_libavb_version_minor;

  /*  12: The size of the signature block. */
  UInt64 authentication_data_block_size;
  /*  20: The size of the auxiliary data block. */
  UInt64 auxiliary_data_block_size;

  /*  28: The verification algorithm used, see |AvbAlgorithmType| enum. */
  UInt32 algorithm_type;

  /*  32: Offset into the "Authentication data" block of hash data. */
  UInt64 hash_offset;
  /*  40: Length of the hash data. */
  UInt64 hash_size;

  /*  48: Offset into the "Authentication data" block of signature data. */
  UInt64 signature_offset;
  /*  56: Length of the signature data. */
  UInt64 signature_size;

  /*  64: Offset into the "Auxiliary data" block of public key data. */
  UInt64 public_key_offset;
  /*  72: Length of the public key data. */
  UInt64 public_key_size;

  /*  80: Offset into the "Auxiliary data" block of public key metadata. */
  UInt64 public_key_metadata_offset;
  /*  88: Length of the public key metadata. Must be set to zero if there
   *  is no public key metadata.
   */
  UInt64 public_key_metadata_size;

  /*  96: Offset into the "Auxiliary data" block of descriptor data. */
  UInt64 descriptors_offset;
  /* 104: Length of descriptor data. */
  UInt64 descriptors_size;

  /* 112: The rollback index which can be used to prevent rollback to
   *  older versions.
   */
  UInt64 rollback_index;

  /* 120: Flags from the AvbVBMetaImageFlags enumeration. This must be
   * set to zero if the vbmeta image is not a top-level image.
   */
  UInt32 flags;

  /* 124: The location of the rollback index defined in this header.
   * Only valid for the main vbmeta. For chained partitions, the rollback
   * index location must be specified in the AvbChainPartitionDescriptor
   * and this value must be set to 0.
   */
  UInt32 rollback_index_location;

  /* 128: The release string from avbtool, e.g. "avbtool 1.0.0" or
   * "avbtool 1.0.0 xyz_board Git-234abde89". Is guaranteed to be NUL
   * terminated. Applications must not make assumptions about how this
   * string is formatted.
   */
  Byte release_string[AVB_RELEASE_STRING_SIZE];

  /* 176: Padding to ensure struct is size AVB_VBMETA_IMAGE_HEADER_SIZE
   * bytes. This must be set to zeroes.
   */
  // Byte reserved[80];
  bool Parse(const Byte *p);
};

bool AvbVBMetaImageHeader::Parse(const Byte *p)
{
  // Byte magic[AVB_MAGIC_LEN];
  if (Get32(p) != 0x41564230) // "AVB0"
    return false;
  G32 (4, required_libavb_version_major);
  if (required_libavb_version_major != AVB_FOOTER_VERSION_MAJOR) // "AVB0"
    return false;
  G32 (8, required_libavb_version_minor);
  G64 (12, authentication_data_block_size);
  G64 (20, auxiliary_data_block_size);
  G32 (28, algorithm_type);
  G64 (32, hash_offset);
  G64 (40, hash_size);
  G64 (48, signature_offset);
  G64 (56, signature_size);
  G64 (64, public_key_offset);
  G64 (72, public_key_size);
  G64 (80, public_key_metadata_offset);
  G64 (88, public_key_metadata_size);
  G64 (96, descriptors_offset);
  G64 (104, descriptors_size);
  G64 (112, rollback_index);
  G32 (120, flags);
  G32 (124, rollback_index_location);
  memcpy(release_string, p + 128, AVB_RELEASE_STRING_SIZE);

  /* 176: Padding to ensure struct is size AVB_VBMETA_IMAGE_HEADER_SIZE
   * bytes. This must be set to zeroes.
   */
  // Byte reserved[80];
  return true;
}


static const unsigned k_Descriptor_Size = 16;

enum AvbDescriptorTag
{
  AVB_DESCRIPTOR_TAG_PROPERTY,
  AVB_DESCRIPTOR_TAG_HASHTREE,
  AVB_DESCRIPTOR_TAG_HASH,
  AVB_DESCRIPTOR_TAG_KERNEL_CMDLINE,
  AVB_DESCRIPTOR_TAG_CHAIN_PARTITION
};

struct AvbDescriptor
{
  UInt64 Tag;
  UInt64 Size;

  void Parse(const Byte *p)
  {
    G64 (0, Tag);
    G64 (8, Size);
  }
};


enum AvbHashtreeDescriptorFlags
{
  AVB_HASHTREE_DESCRIPTOR_FLAGS_DO_NOT_USE_AB = (1 << 0),
  AVB_HASHTREE_DESCRIPTOR_FLAGS_CHECK_AT_MOST_ONCE = (1 << 1)
};

/* A descriptor containing information about a dm-verity hashtree.
 *
 * Hash-trees are used to verify large partitions typically containing
 * file systems. See
 * https://gitlab.com/cryptsetup/cryptsetup/wikis/DMVerity for more
 * information about dm-verity.
 *
 * Following this struct are |partition_name_len| bytes of the
 * partition name (UTF-8 encoded), |salt_len| bytes of salt, and then
 * |root_digest_len| bytes of the root digest.
 *
 * The |reserved| field is for future expansion and must be set to NUL
 * bytes.
 *
 * Changes in v1.1:
 *   - flags field is added which supports AVB_HASHTREE_DESCRIPTOR_FLAGS_USE_AB
 *   - digest_len may be zero, which indicates the use of a persistent digest
 */

static const unsigned k_Hashtree_Size_Min = 164;

struct AvbHashtreeDescriptor
{
  UInt32 dm_verity_version;
  UInt64 image_size;
  UInt64 tree_offset;
  UInt64 tree_size;
  UInt32 data_block_size;
  UInt32 hash_block_size;
  UInt32 fec_num_roots;
  UInt64 fec_offset;
  UInt64 fec_size;
  Byte hash_algorithm[32];
  UInt32 partition_name_len;
  UInt32 salt_len;
  UInt32 root_digest_len;
  UInt32 flags;
  Byte reserved[60];
  void Parse(const Byte *p)
  {
    G32 (0, dm_verity_version);
    G64 (4, image_size);
    G64 (12, tree_offset);
    G64 (20, tree_size);
    G32 (28, data_block_size);
    G32 (32, hash_block_size);
    G32 (36, fec_num_roots);
    G64 (40, fec_offset);
    G64 (48, fec_size);
    memcpy(hash_algorithm, p + 56, 32);
    G32 (88, partition_name_len);
    G32 (92, salt_len);
    G32 (96, root_digest_len);
    G32 (100, flags);
  }
};

static const unsigned k_PropertyDescriptor_Size_Min = 16;

struct AvbPropertyDescriptor
{
  UInt64 key_num_bytes;
  UInt64 value_num_bytes;
  
  void Parse(const Byte *p)
  {
    G64 (0, key_num_bytes);
    G64 (8, value_num_bytes);
  }
};

Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  // UInt64 _startOffset;
  UInt64 _phySize;

  CFooter Footer;
  AString Name;
  const char *Ext;

  HRESULT Open2(IInStream *stream);

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override
  {
    if (index != 0)
      return NExtract::NOperationResult::kUnavailable;
    // pos = _startOffset;
    pos = 0;
    size = Footer.original_image_size;
    return NExtract::NOperationResult::kOK;
  }
};


HRESULT CHandler::Open2(IInStream *stream)
{
  UInt64 fileSize;
  {
    Byte buf[kFooterSize];
    RINOK(InStream_GetSize_SeekToEnd(stream, fileSize))
    if (fileSize < kFooterSize)
      return S_FALSE;
    RINOK(InStream_SeekSet(stream, fileSize - kFooterSize))
    RINOK(ReadStream_FALSE(stream, buf, kFooterSize))
    if (memcmp(buf, k_Signature, k_SignatureSize) != 0)
      return S_FALSE;
    Footer.Parse(buf);
    if (Footer.vbmeta_size > VBMETA_MAX_SIZE ||
        Footer.vbmeta_size < AVB_VBMETA_IMAGE_HEADER_SIZE)
      return S_FALSE;
    for (unsigned i = 36; i < kFooterSize; i++)
      if (buf[i] != 0)
        return S_FALSE;
  }
  {
    CByteBuffer buf;
    buf.Alloc((size_t)Footer.vbmeta_size);
    RINOK(InStream_SeekSet(stream, Footer.vbmeta_offset))
    RINOK(ReadStream_FALSE(stream, buf, (size_t)Footer.vbmeta_size))

    AvbVBMetaImageHeader meta;
    if (!meta.Parse(buf))
      return S_FALSE;

    unsigned offset = (unsigned)AVB_VBMETA_IMAGE_HEADER_SIZE;
    unsigned rem = (unsigned)(Footer.vbmeta_size - offset);
    
    if (meta.authentication_data_block_size != 0)
    {
      if (rem < meta.authentication_data_block_size)
        return S_FALSE;
      const unsigned u = (unsigned)meta.authentication_data_block_size;
      offset += u;
      rem -= u;
    }

    if (rem < meta.descriptors_offset ||
        rem - meta.descriptors_offset < meta.descriptors_size)
      return S_FALSE;
    rem = (unsigned)meta.descriptors_size;
    while (rem != 0)
    {
      if (rem < k_Descriptor_Size)
        return S_FALSE;
      AvbDescriptor desc;
      desc.Parse(buf + offset);
      offset += k_Descriptor_Size;
      rem -= k_Descriptor_Size;
      if (desc.Size > rem)
        return S_FALSE;
      const unsigned descSize = (unsigned)desc.Size;
      if (desc.Tag == AVB_DESCRIPTOR_TAG_HASHTREE)
      {
        if (descSize < k_Hashtree_Size_Min)
          return S_FALSE;
        AvbHashtreeDescriptor ht;
        ht.Parse(buf + offset);
        unsigned pos = k_Hashtree_Size_Min;

        if (pos + ht.partition_name_len > descSize)
          return S_FALSE;
        Name.Empty(); // UTF-8
        AddNameToString(Name, buf + offset + pos, ht.partition_name_len, false);
        pos += ht.partition_name_len;
        
        if (pos + ht.salt_len > descSize)
          return S_FALSE;
        CByteBuffer salt;
        salt.CopyFrom(buf + offset + pos, ht.salt_len);
        pos += ht.salt_len;
        
        if (pos + ht.root_digest_len > descSize)
          return S_FALSE;
        CByteBuffer digest;
        digest.CopyFrom(buf + offset + pos, ht.root_digest_len);
        pos += ht.root_digest_len;
        // what is that digest?
      }
      else if (desc.Tag == AVB_DESCRIPTOR_TAG_PROPERTY)
      {
        if (descSize < k_PropertyDescriptor_Size_Min + 2)
          return S_FALSE;
        AvbPropertyDescriptor pt;
        pt.Parse(buf + offset);
        unsigned pos = k_PropertyDescriptor_Size_Min;
        
        if (pt.key_num_bytes > descSize - pos - 1)
          return S_FALSE;
        AString key; // UTF-8
        AddNameToString(key, buf + offset + pos, (unsigned)pt.key_num_bytes, false);
        pos += (unsigned)pt.key_num_bytes + 1;

        if (descSize < pos)
          return S_FALSE;
        if (pt.value_num_bytes > descSize - pos - 1)
          return S_FALSE;
        AString value; // UTF-8
        AddNameToString(value, buf + offset + pos, (unsigned)pt.value_num_bytes, false);
        pos += (unsigned)pt.value_num_bytes + 1;
      }
      offset += descSize;
      rem -= descSize;
    }

    _phySize = fileSize;

    // _startOffset = 0;
    return S_OK;
  }
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  try
  {
    if (Open2(stream) != S_OK)
      return S_FALSE;
    _stream = stream;

    {
      CMyComPtr<ISequentialInStream> parseStream;
      if (GetStream(0, &parseStream) == S_OK && parseStream)
      {
        const size_t kParseSize = 1 << 11;
        Byte buf[kParseSize];
        if (ReadStream_FAIL(parseStream, buf, kParseSize) == S_OK)
        {
          UInt64 extSize;
          if (NExt::IsArc_Ext_PhySize(buf, kParseSize, &extSize) == k_IsArc_Res_YES)
            if (extSize == Footer.original_image_size)
              Ext = "ext";
        }
      }
    }
  }
  catch(...) { return S_FALSE; }
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _stream.Release();
  // _startOffset = 0;
  _phySize = 0;
  Ext = NULL;
  Name.Empty();
  return S_OK;
}


static const Byte kArcProps[] =
{
  kpidName
};

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize,
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps


Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidName:
    {
      if (!Name.IsEmpty())
      {
        AString s (Name);
        s += ".avb";
        prop = s;
      }
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  switch (propID)
  {
    case kpidPath:
    {
      if (!Name.IsEmpty())
      {
        AString s (Name);
        s += '.';
        s += Ext ? Ext : "img";
        prop = s;
      }
      break;
    }
    case kpidPackSize:
    case kpidSize:
      prop = Footer.original_image_size;
      break;
    case kpidExtension: prop = (Ext ? Ext : "img"); break;
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = 1;
  return S_OK;
}


REGISTER_ARC_I_NO_SIG(
  "AVB", "avb img", NULL, 0xc0,
  /* k_Signature, */
  0,
  /* NArcInfoFlags::kUseGlobalOffset | */
  NArcInfoFlags::kBackwardOpen
  ,
  NULL)

}}
