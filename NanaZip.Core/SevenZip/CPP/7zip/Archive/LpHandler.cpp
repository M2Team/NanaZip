// LpHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/Sha256.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define G16(_offs_, dest) dest = Get16(p + (_offs_));
#define G32(_offs_, dest) dest = Get32(p + (_offs_));
#define G64(_offs_, dest) dest = Get64(p + (_offs_));

using namespace NWindows;

namespace NArchive {

namespace NExt {
API_FUNC_IsArc IsArc_Ext_PhySize(const Byte *p, size_t size, UInt64 *phySize);
}

namespace NLp {

/*
Android 10+ use Android's Dynamic Partitions to allow the
different read-only system partitions (e.g. system, vendor, product)
to share the same pool of storage space (as LVM in Linux).
Name for partition: "super" (for GPT) or "super.img" (for file).
Dynamic Partition Tools: lpmake
All partitions that are A/B-ed should be named as follows (slots are always named a, b, etc.):
boot_a, boot_b, system_a, system_b, vendor_a, vendor_b.
*/

#define LP_METADATA_MAJOR_VERSION 10
// #define LP_METADATA_MINOR_VERSION_MIN 0
// #define LP_METADATA_MINOR_VERSION_MAX 2

// #define LP_SECTOR_SIZE 512
static const unsigned kSectorSizeLog = 9;

/* Amount of space reserved at the start of every super partition to avoid
 * creating an accidental boot sector. */
#define LP_PARTITION_RESERVED_BYTES 4096
#define LP_METADATA_GEOMETRY_SIZE 4096
#define LP_METADATA_HEADER_MAGIC 0x414C5030

static const unsigned k_SignatureSize = 8;
static const Byte k_Signature[k_SignatureSize] =
  { 0x67, 0x44, 0x6c, 0x61, 0x34, 0, 0, 0 };

// The length (36) is the same as the maximum length of a GPT partition name.
static const unsigned kNameLen = 36;

static void AddName36ToString(AString &s, const char *name, bool strictConvert)
{
  for (unsigned i = 0; i < kNameLen; i++)
  {
    char c = name[i];
    if (c == 0)
      return;
    if (strictConvert && c < 32)
      c = '_';
    s += c;
  }
}


static const unsigned k_Geometry_Size = 0x34;

// LpMetadataGeometry
struct CGeometry
{
  // UInt32 magic;
  // UInt32 struct_size;
  // Byte checksum[32];   /*  SHA256 checksum of this struct, with this field set to 0. */
  
  /* Maximum amount of space a single copy of the metadata can use,
     a multiple of LP_SECTOR_SIZE. */
  UInt32 metadata_max_size;
  
  /* Number of copies of the metadata to keep.
     For Non-A/B: 1, For A/B: 2, for A/B/C: 3.
     A backup copy of each slot is kept */
  UInt32 metadata_slot_count;
  
  /* minimal alignment for partition and extent sizes, a multiple of LP_SECTOR_SIZE. */
  UInt32 logical_block_size;

  bool Parse(const Byte *p)
  {
    G32 (40, metadata_max_size)
    G32 (44, metadata_slot_count)
    G32 (48, logical_block_size)
    if (metadata_slot_count == 0 || metadata_slot_count >= ((UInt32)1 << 20))
      return false;
    if (metadata_max_size == 0)
      return false;
    if ((metadata_max_size & (((UInt32)1 << kSectorSizeLog) - 1)) != 0)
      return false;
    return true;
  }

  UInt64 GetTotalMetadataSize() const
  {
    // there are 2 copies of GEOMETRY and METADATA slots
    return LP_PARTITION_RESERVED_BYTES +
           LP_METADATA_GEOMETRY_SIZE * 2 +
           ((UInt64)metadata_max_size * metadata_slot_count) * 2;
  }
};



// LpMetadataTableDescriptor
struct CDescriptor
{
  UInt32 offset;        /*  Location of the table, relative to end of the metadata header. */
  UInt32 num_entries;   /*  Number of entries in the table. */
  UInt32 entry_size;    /*  Size of each entry in the table, in bytes. */

  void Parse(const Byte *p)
  {
    G32 (0, offset)
    G32 (4, num_entries)
    G32 (8, entry_size)
  }
 
  bool CheckLimits(UInt32 limit) const
  {
    if (entry_size == 0)
      return false;
    const UInt32 size = num_entries * entry_size;
    if (size / entry_size != num_entries)
      return false;
    if (offset > limit || limit - offset < size)
      return false;
    return true;
  }
};


// #define LP_PARTITION_ATTR_NONE 0x0
// #define LP_PARTITION_ATTR_READONLY (1 << 0)

/* This flag is only intended to be used with super_empty.img and super.img on
 * retrofit devices. On these devices there are A and B super partitions, and
 * we don't know ahead of time which slot the image will be applied to.
 *
 * If set, the partition name needs a slot suffix applied. The slot suffix is
 * determined by the metadata slot number (0 = _a, 1 = _b).
 */
// #define LP_PARTITION_ATTR_SLOT_SUFFIXED (1 << 1)

/* This flag is applied automatically when using MetadataBuilder::NewForUpdate.
 * It signals that the partition was created (or modified) for a snapshot-based
 * update. If this flag is not present, the partition was likely flashed via
 * fastboot.
 */
// #define LP_PARTITION_ATTR_UPDATED (1 << 2)

/* This flag marks a partition as disabled. It should not be used or mapped. */
// #define LP_PARTITION_ATTR_DISABLED (1 << 3)

static const char * const g_PartitionAttr[] =
{
    "READONLY"
  , "SLOT_SUFFIXED"
  , "UPDATED"
  , "DISABLED"
};

static unsigned const k_MetaPartition_Size = 52;

// LpMetadataPartition
struct CPartition
{
  /* ASCII characters: alphanumeric or _. at least one ASCII character,
     (name) must be unique across all partition names. */
  char name[kNameLen];
  
  UInt32 attributes;   /* (LP_PARTITION_ATTR_*). */

  /* Index of the first extent owned by this partition. The extent will
   * start at logical sector 0. Gaps between extents are not allowed. */
  UInt32 first_extent_index;
  
  /* Number of extents in the partition. Every partition must have at least one extent. */
  UInt32 num_extents;

  /* Group this partition belongs to. */
  UInt32 group_index;

  void Parse(const Byte *p)
  {
    memcpy(name, p, kNameLen);
    G32 (36, attributes)
    G32 (40, first_extent_index)
    G32 (44, num_extents)
    G32 (48, group_index)
  }

  // calced properties:
  UInt32 MethodsMask;
  UInt64 NumSectors;
  UInt64 NumSectors_Pack;
  const char *Ext;

  UInt64 GetSize() const { return NumSectors << kSectorSizeLog; }
  UInt64 GetPackSize() const { return NumSectors_Pack << kSectorSizeLog; }
  
  CPartition():
      MethodsMask(0),
      NumSectors(0),
      NumSectors_Pack(0),
      Ext(NULL)
      {}
};




#define LP_TARGET_TYPE_LINEAR 0
/* This extent is a dm-zero target. The index is ignored and must be 0. */
#define LP_TARGET_TYPE_ZERO 1

static const char * const g_Methods[] =
{
    "RAW" // "LINEAR"
  , "ZERO"
};

static unsigned const k_MetaExtent_Size = 24;

// LpMetadataExtent
struct CExtent
{
  UInt64 num_sectors;  /*  Length in 512-byte sectors. */
  UInt32 target_type;  /*  Target type for device-mapper (LP_TARGET_TYPE_*). */

  /* for LINEAR: The sector on the physical partition that this extent maps onto.
     for ZERO:   must be 0. */
  UInt64 target_data;

  /* for LINEAR: index into the block devices table.
     for ZERO:   must be 0. */
  UInt32 target_source;

  bool IsRAW() const { return target_type == LP_TARGET_TYPE_LINEAR; }

  void Parse(const Byte *p)
  {
    G64 (0, num_sectors)
    G32 (8, target_type)
    G64 (12, target_data)
    G32 (20, target_source)
  }
};


/* This flag is only intended to be used with super_empty.img and super.img on
 * retrofit devices. If set, the group needs a slot suffix to be interpreted
 * correctly. The suffix is automatically applied by ReadMetadata().
 */
// #define LP_GROUP_SLOT_SUFFIXED (1 << 0)
static unsigned const k_Group_Size = 48;

// LpMetadataPartitionGroup
struct CGroup
{
  char name[kNameLen];
  UInt32 flags;  /* (LP_GROUP_*). */
  UInt64 maximum_size; /* Maximum size in bytes. If 0, the group has no maximum size. */

  void Parse(const Byte *p)
  {
    memcpy(name, p, kNameLen);
    G32 (36, flags)
    G64 (40, maximum_size)
  }
};




/* This flag is only intended to be used with super_empty.img and super.img on
 * retrofit devices. On these devices there are A and B super partitions, and
 * we don't know ahead of time which slot the image will be applied to.
 *
 * If set, the block device needs a slot suffix applied before being used with
 * IPartitionOpener. The slot suffix is determined by the metadata slot number
 * (0 = _a, 1 = _b).
 */
// #define LP_BLOCK_DEVICE_SLOT_SUFFIXED (1 << 0)

static unsigned const k_Device_Size = 64;

/* This struct defines an entry in the block_devices table. There must be at
 * least one device, and the first device must represent the partition holding
 * the super metadata.
 */
// LpMetadataBlockDevice
struct CDevice
{
    /* 0: First usable sector for allocating logical partitions. this will be
     * the first sector after the initial geometry blocks, followed by the
     * space consumed by metadata_max_size*metadata_slot_count*2.
     */
  UInt64 first_logical_sector;

    /* 8: Alignment for defining partitions or partition extents. For example,
     * an alignment of 1MiB will require that all partitions have a size evenly
     * divisible by 1MiB, and that the smallest unit the partition can grow by
     * is 1MiB.
     *
     * Alignment is normally determined at runtime when growing or adding
     * partitions. If for some reason the alignment cannot be determined, then
     * this predefined alignment in the geometry is used instead. By default
     * it is set to 1MiB.
     */
  UInt32 alignment;

    /* 12: Alignment offset for "stacked" devices. For example, if the "super"
     * partition itself is not aligned within the parent block device's
     * partition table, then we adjust for this in deciding where to place
     * |first_logical_sector|.
     *
     * Similar to |alignment|, this will be derived from the operating system.
     * If it cannot be determined, it is assumed to be 0.
     */
  UInt32 alignment_offset;

    /* 16: Block device size, as specified when the metadata was created. This
     * can be used to verify the geometry against a target device.
     */
  UInt64 size;

    /* 24: Partition name in the GPT*/
  char partition_name[kNameLen];

    /* 60: Flags (see LP_BLOCK_DEVICE_* flags below). */
  UInt32 flags;

  void Parse(const Byte *p)
  {
    memcpy(partition_name, p + 24, kNameLen);
    G64 (0, first_logical_sector)
    G32 (8, alignment)
    G32 (12, alignment_offset)
    G64 (16, size)
    G32 (60, flags)
  }
};


/* This device uses Virtual A/B. Note that on retrofit devices, the expanded
 * header may not be present.
 */
// #define LP_HEADER_FLAG_VIRTUAL_AB_DEVICE 0x1

static const char * const g_Header_Flags[] =
{
  "VIRTUAL_AB"
};


static const unsigned k_LpMetadataHeader10_size = 128;
static const unsigned k_LpMetadataHeader12_size = 256;

struct LpMetadataHeader
{
  /*  0: Four bytes equal to LP_METADATA_HEADER_MAGIC. */
  UInt32 magic;
  
  /*  4: Version number required to read this metadata. If the version is not
   * equal to the library version, the metadata should be considered
   * incompatible.
   */
  UInt16 major_version;
  
  /*  6: Minor version. A library supporting newer features should be able to
   * read metadata with an older minor version. However, an older library
   * should not support reading metadata if its minor version is higher.
   */
  UInt16 minor_version;
  
  /*  8: The size of this header struct. */
  UInt32 header_size;
  
  /* 12: SHA256 checksum of the header, up to |header_size| bytes, computed as
   * if this field were set to 0.
   */
  // Byte header_checksum[32];
  
  /* 44: The total size of all tables. This size is contiguous; tables may not
   * have gaps in between, and they immediately follow the header.
   */
  UInt32 tables_size;
  
  /* 48: SHA256 checksum of all table contents. */
  Byte tables_checksum[32];
  
  /* 80: Partition table descriptor. */
  CDescriptor partitions;
  /* 92: Extent table descriptor. */
  CDescriptor extents;
  /* 104: Updateable group descriptor. */
  CDescriptor groups;
  /* 116: Block device table. */
  CDescriptor block_devices;
  
  /* Everything past here is header version 1.2+, and is only included if
   * needed. When liblp supporting >= 1.2 reads a < 1.2 header, it must
   * zero these additional fields.
   */
  
  /* 128: See LP_HEADER_FLAG_ constants for possible values. Header flags are
   * independent of the version number and intended to be informational only.
   * New flags can be added without bumping the version.
   */
  // UInt32 flags;
  
  /* 132: Reserved (zero), pad to 256 bytes. */
  // Byte reserved[124];

  void Parse128(const Byte *p)
  {
    G32 (0, magic)
    G16 (4, major_version)
    G16 (6, minor_version)
    G32 (8, header_size)
    // Byte header_checksum[32];
    G32 (44, tables_size)
    memcpy (tables_checksum, p + 48, 32);
    partitions.Parse(p + 80);
    extents.Parse(p + 92);
    groups.Parse(p + 104);
    block_devices.Parse(p + 116);
    /* Everything past here is header version 1.2+, and is only included if
     * needed. When liblp supporting >= 1.2 reads a < 1.2 header, it must
     * zero these additional fields.
     */
  }
};


static bool CheckSha256(const Byte *data, size_t size, const Byte *checksum)
{
  CSha256 sha;
  Sha256_Init(&sha);
  Sha256_Update(&sha, data, size);
  Byte calced[32];
  Sha256_Final(&sha, calced);
  return memcmp(checksum, calced, 32) == 0;
}

static bool CheckSha256_csOffset(Byte *data, size_t size, unsigned hashOffset)
{
  Byte checksum[32];
  Byte *shaData = &data[hashOffset];
  memcpy(checksum, shaData, 32);
  memset(shaData, 0, 32);
  return CheckSha256(data, size, checksum);
}



Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CRecordVector<CPartition> _items;
  CRecordVector<CExtent> Extents;

  CMyComPtr<IInStream> _stream;
  UInt64 _totalSize;
  // UInt64 _usedSize;
  // UInt64 _headersSize;

  CGeometry geom;
  UInt16 Major_version;
  UInt16 Minor_version;
  UInt32 Flags;

  Int32 _mainFileIndex;
  UInt32 MethodsMask;
  bool _headerWarning;
  AString GroupsString;
  AString DevicesString;
  AString DeviceArcName;

  HRESULT Open2(IInStream *stream);
};


static void AddComment_UInt64(AString &s, const char *name, UInt64 val)
{
  s.Add_Space();
  s += name;
  s += '=';
  s.Add_UInt64(val);
}


static bool IsBufZero(const Byte *data, size_t size)
{
  for (size_t i = 0; i < size; i += 4)
    if (*(const UInt32 *)(const void *)(data + i) != 0)
      return false;
  return true;
}


HRESULT CHandler::Open2(IInStream *stream)
{
  RINOK(InStream_SeekSet(stream, LP_PARTITION_RESERVED_BYTES))
  {
    Byte buf[k_Geometry_Size];
    RINOK(ReadStream_FALSE(stream, buf, k_Geometry_Size))
    if (memcmp(buf, k_Signature, k_SignatureSize) != 0)
      return S_FALSE;
    if (!geom.Parse(buf))
      return S_FALSE;
    if (!CheckSha256_csOffset(buf, k_Geometry_Size, 8))
      return S_FALSE;
  }

  CByteBuffer buffer;
  RINOK(InStream_SeekToBegin(stream))
  buffer.Alloc(LP_METADATA_GEOMETRY_SIZE * 2);
  {
    // buffer.Size() >= LP_PARTITION_RESERVED_BYTES
    RINOK(ReadStream_FALSE(stream, buffer, LP_PARTITION_RESERVED_BYTES))
    if (!IsBufZero(buffer, LP_PARTITION_RESERVED_BYTES))
    {
      _headerWarning = true;
      // return S_FALSE;
    }
  }

  RINOK(ReadStream_FALSE(stream, buffer, LP_METADATA_GEOMETRY_SIZE * 2))
  // we check that 2 copies of GEOMETRY are identical:
  if (memcmp(buffer, buffer + LP_METADATA_GEOMETRY_SIZE, LP_METADATA_GEOMETRY_SIZE) != 0
      || !IsBufZero(buffer + k_Geometry_Size, LP_METADATA_GEOMETRY_SIZE - k_Geometry_Size))
  {
    _headerWarning = true;
    // return S_FALSE;
  }

  RINOK(ReadStream_FALSE(stream, buffer, k_LpMetadataHeader10_size))
  LpMetadataHeader header;
  header.Parse128(buffer);
  if (header.magic != LP_METADATA_HEADER_MAGIC ||
      header.major_version != LP_METADATA_MAJOR_VERSION ||
      header.header_size < k_LpMetadataHeader10_size)
    return S_FALSE;
  Flags = 0;
  if (header.header_size > k_LpMetadataHeader10_size)
  {
    if (header.header_size != k_LpMetadataHeader12_size)
      return S_FALSE;
    RINOK(ReadStream_FALSE(stream, buffer + k_LpMetadataHeader10_size,
        header.header_size - k_LpMetadataHeader10_size))
    Flags = Get32(buffer + k_LpMetadataHeader10_size);
  }
  Major_version = header.major_version;
  Minor_version = header.minor_version;

  if (!CheckSha256_csOffset(buffer, header.header_size, 12))
    return S_FALSE;

  if (geom.metadata_max_size < header.tables_size ||
      geom.metadata_max_size - header.tables_size < header.header_size)
    return S_FALSE;

  buffer.AllocAtLeast(header.tables_size);
  RINOK(ReadStream_FALSE(stream, buffer, header.tables_size))

  const UInt64 totalMetaSize = geom.GetTotalMetadataSize();
  // _headersSize = _totalSize;
  _totalSize = totalMetaSize;

  if (!CheckSha256(buffer, header.tables_size, header.tables_checksum))
    return S_FALSE;

  {
    const CDescriptor &d = header.partitions;
    if (!d.CheckLimits(header.tables_size))
      return S_FALSE;
    if (d.entry_size != k_MetaPartition_Size)
      return S_FALSE;
    for (UInt32 i = 0; i < d.num_entries; i++)
    {
      CPartition part;
      part.Parse(buffer + d.offset + i * d.entry_size);
      const UInt32 extLimit = part.first_extent_index + part.num_extents;
      if (extLimit < part.first_extent_index ||
          extLimit > header.extents.num_entries ||
          part.group_index >= header.groups.num_entries)
        return S_FALSE;
      _items.Add(part);
    }
  }
  {
    const CDescriptor &d = header.extents;
    if (!d.CheckLimits(header.tables_size))
      return S_FALSE;
    if (d.entry_size != k_MetaExtent_Size)
      return S_FALSE;
    for (UInt32 i = 0; i < d.num_entries; i++)
    {
      CExtent e;
      e.Parse(buffer + d.offset + i * d.entry_size);
      // if (e.target_type > LP_TARGET_TYPE_ZERO) return S_FALSE;
      if (e.IsRAW())
      {
        if (e.target_source >= header.block_devices.num_entries)
          return S_FALSE;
        const UInt64 endSector = e.target_data + e.num_sectors;
        const UInt64 endOffset = endSector << kSectorSizeLog;
        if (_totalSize < endOffset)
          _totalSize = endOffset;
      }
      MethodsMask |= (UInt32)1 << e.target_type;
      Extents.Add(e);
    }
  }
  
  // _usedSize = _totalSize;
  {
    const CDescriptor &d = header.groups;
    if (!d.CheckLimits(header.tables_size))
      return S_FALSE;
    if (d.entry_size != k_Group_Size)
      return S_FALSE;
    AString s;
    for (UInt32 i = 0; i < d.num_entries; i++)
    {
      CGroup g;
      g.Parse(buffer + d.offset + i * d.entry_size);
      if (_totalSize < g.maximum_size)
        _totalSize = g.maximum_size;
      s += "  ";
      AddName36ToString(s, g.name, true);
      AddComment_UInt64(s, "maximum_size", g.maximum_size);
      AddComment_UInt64(s, "flags", g.flags);
      s.Add_LF();
    }
    GroupsString = s;
  }

  {
    const CDescriptor &d = header.block_devices;
    if (!d.CheckLimits(header.tables_size))
      return S_FALSE;
    if (d.entry_size != k_Device_Size)
      return S_FALSE;
    AString s;
    // CRecordVector<CDevice> devices;
    for (UInt32 i = 0; i < d.num_entries; i++)
    {
      CDevice v;
      v.Parse(buffer + d.offset + i * d.entry_size);
      // if (i == 0)
      {
        // it's super_device is first device;
        if (totalMetaSize > (v.first_logical_sector << kSectorSizeLog))
          return S_FALSE;
      }
      if (_totalSize < v.size)
        _totalSize = v.size;
      s += "  ";
      if (i == 0)
        AddName36ToString(DeviceArcName, v.partition_name, true);
      // devices.Add(v);
      AddName36ToString(s, v.partition_name, true);
      AddComment_UInt64(s, "size", v.size);
      AddComment_UInt64(s, "first_logical_sector", v.first_logical_sector);
      AddComment_UInt64(s, "alignment", v.alignment);
      AddComment_UInt64(s, "alignment_offset", v.alignment_offset);
      AddComment_UInt64(s, "flags", v.flags);
      s.Add_LF();
    }
    DevicesString = s;
  }

  {
    FOR_VECTOR (i, _items)
    {
      CPartition &part = _items[i];
      if (part.first_extent_index > Extents.Size() ||
          part.num_extents > Extents.Size() - part.first_extent_index)
        return S_FALSE;

      UInt64 numSectors = 0;
      UInt64 numSectors_Pack = 0;
      UInt32 methods = 0;
      for (UInt32 k = 0; k < part.num_extents; k++)
      {
        const CExtent &e = Extents[part.first_extent_index + k];
        numSectors += e.num_sectors;
        if (e.IsRAW())
          numSectors_Pack += e.num_sectors;
        methods |= (UInt32)1 << e.target_type;
      }
      part.NumSectors = numSectors;
      part.NumSectors_Pack = numSectors_Pack;
      part.MethodsMask = methods;
    }
  }

  return S_OK;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(stream))
  _stream = stream;

  int mainFileIndex = -1;
  unsigned numNonEmptyParts = 0;

  FOR_VECTOR (fileIndex, _items)
  {
    CPartition &item = _items[fileIndex];
    if (item.NumSectors != 0)
    {
      mainFileIndex = (int)fileIndex;
      numNonEmptyParts++;
      CMyComPtr<ISequentialInStream> parseStream;
      if (GetStream(fileIndex, &parseStream) == S_OK && parseStream)
      {
        const size_t kParseSize = 1 << 11;
        Byte buf[kParseSize];
        if (ReadStream_FAIL(parseStream, buf, kParseSize) == S_OK)
        {
          UInt64 extSize;
          if (NExt::IsArc_Ext_PhySize(buf, kParseSize, &extSize) == k_IsArc_Res_YES)
            if (extSize == item.GetSize())
              item.Ext = "ext";
        }
      }
    }
  }
  if (numNonEmptyParts == 1)
    _mainFileIndex = mainFileIndex;

  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _totalSize = 0;
  // _usedSize = 0;
  // _headersSize = 0;
  _items.Clear();
  Extents.Clear();
  _stream.Release();
  _mainFileIndex = -1;
  _headerWarning = false;
  MethodsMask = 0;
  GroupsString.Empty();
  DevicesString.Empty();
  DeviceArcName.Empty();
  return S_OK;
}


static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidCharacts,
  kpidMethod,
  kpidNumBlocks,
  kpidOffset
};

static const Byte kArcProps[] =
{
  kpidUnpackVer,
  kpidMethod,
  kpidClusterSize,
  // kpidHeadersSize,
  // kpidFreeSpace,
  kpidName,
  kpidComment
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile:
    {
      if (_mainFileIndex >= 0)
        prop = (UInt32)_mainFileIndex;
      break;
    }
    case kpidPhySize: prop = _totalSize; break;

    // case kpidFreeSpace: if (_usedSize != 0) prop = _totalSize - _usedSize; break;
    // case kpidHeadersSize: prop = _headersSize; break;

    case kpidMethod:
    {
      const UInt32 m = MethodsMask;
      if (m != 0)
      {
        FLAGS_TO_PROP(g_Methods, m, prop);
      }
      break;
    }

    case kpidUnpackVer:
    {
      AString s;
      s.Add_UInt32(Major_version);
      s.Add_Dot();
      s.Add_UInt32(Minor_version);
      prop = s;
      break;
    }

    case kpidClusterSize:
      prop = geom.logical_block_size;
      break;

    case kpidComment:
    {
      AString s;

      s += "metadata_slot_count: ";
      s.Add_UInt32(geom.metadata_slot_count);
      s.Add_LF();

      s += "metadata_max_size: ";
      s.Add_UInt32(geom.metadata_max_size);
      s.Add_LF();

      if (Flags != 0)
      {
        s += "flags: ";
        s += FlagsToString(g_Header_Flags, Z7_ARRAY_SIZE(g_Header_Flags), Flags);
        s.Add_LF();
      }

      if (!GroupsString.IsEmpty())
      {
        s += "Groups:";
        s.Add_LF();
        s += GroupsString;
      }
      
      if (!DevicesString.IsEmpty())
      {
        s += "BlockDevices:";
        s.Add_LF();
        s += DevicesString;
      }
      
      if (!s.IsEmpty())
        prop = s;
      break;
    }

    case kpidName:
      if (!DeviceArcName.IsEmpty())
        prop = DeviceArcName + ".lpimg";
      break;

    case kpidWarningFlags:
      if (_headerWarning)
      {
        UInt32 v = kpv_ErrorFlags_HeadersError;
        prop = v;
      }
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CPartition &item = _items[index];

  switch (propID)
  {
    case kpidPath:
    {
      AString s;
      AddName36ToString(s, item.name, false);
      if (s.IsEmpty())
        s.Add_UInt32(index);
      if (item.num_extents != 0)
      {
        s.Add_Dot();
        s += (item.Ext ? item.Ext : "img");
      }
      prop = s;
      break;
    }
    
    case kpidSize: prop = item.GetSize(); break;
    case kpidPackSize: prop = item.GetPackSize(); break;
    case kpidNumBlocks: prop = item.num_extents; break;
    case kpidMethod:
    {
      const UInt32 m = item.MethodsMask;
      if (m != 0)
      {
        FLAGS_TO_PROP(g_Methods, m, prop);
      }
      break;
    }
    case kpidOffset:
      if (item.num_extents != 0)
        if (item.first_extent_index < Extents.Size())
          prop = Extents[item.first_extent_index].target_data << kSectorSizeLog;
      break;

    case kpidCharacts:
    {
      AString s;
      s += "group:";
      s.Add_UInt32(item.group_index);
      s.Add_Space();
      s += FlagsToString(g_PartitionAttr, Z7_ARRAY_SIZE(g_PartitionAttr), item.attributes);
      prop = s;
      break;
    }
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}



Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  
  const CPartition &item = _items[index];

  if (item.first_extent_index > Extents.Size()
      || item.num_extents > Extents.Size() - item.first_extent_index)
    return S_FALSE;

  if (item.num_extents == 0)
    return CreateLimitedInStream(_stream, 0, 0, stream);

  if (item.num_extents == 1)
  {
    const CExtent &e = Extents[item.first_extent_index];
    if (e.IsRAW())
    {
      const UInt64 pos = e.target_data << kSectorSizeLog;
      if ((pos >> kSectorSizeLog) != e.target_data)
        return S_FALSE;
      const UInt64 size = item.GetSize();
      if (pos + size < pos)
        return S_FALSE;
      return CreateLimitedInStream(_stream, pos, size, stream);
    }
  }

  CExtentsStream *extentStreamSpec = new CExtentsStream();
  CMyComPtr<ISequentialInStream> extentStream = extentStreamSpec;

  // const unsigned kNumDebugExtents = 10;
  extentStreamSpec->Extents.Reserve(item.num_extents + 1
      // + kNumDebugExtents
      );

  UInt64 virt = 0;
  for (UInt32 k = 0; k < item.num_extents; k++)
  {
    const CExtent &e = Extents[item.first_extent_index + k];

    CSeekExtent se;
    {
      const UInt64 numSectors = e.num_sectors;
      if (numSectors == 0)
      {
        continue;
        // return S_FALSE;
      }
      const UInt64 numBytes = numSectors << kSectorSizeLog;
      if ((numBytes >> kSectorSizeLog) != numSectors)
        return S_FALSE;
      if (numBytes >= ((UInt64)1 << 63) - virt)
        return S_FALSE;
      
      se.Virt = virt;
      virt += numBytes;
    }

    const UInt64 phySector = e.target_data;
    if (e.target_type == LP_TARGET_TYPE_ZERO)
    {
      if (phySector != 0)
        return S_FALSE;
      se.SetAs_ZeroFill();
    }
    else if (e.target_type == LP_TARGET_TYPE_LINEAR)
    {
      se.Phy = phySector << kSectorSizeLog;
      if ((se.Phy >> kSectorSizeLog) != phySector)
        return S_FALSE;
      if (se.Phy >= ((UInt64)1 << 63))
        return S_FALSE;
    }
    else
      return S_FALSE;

    extentStreamSpec->Extents.AddInReserved(se);

    /*
    {
      // for debug
      const UInt64 kAdd = (e.num_sectors << kSectorSizeLog) / kNumDebugExtents;
      for (unsigned i = 0; i < kNumDebugExtents; i++)
      {
        se.Phy += kAdd;
        // se.Phy += (UInt64)1 << 63; // for debug
        // se.Phy += 1; // for debug
        se.Virt += kAdd;
        extentStreamSpec->Extents.AddInReserved(se);
      }
    }
    */
  }

  CSeekExtent se;
  se.Phy = 0;
  se.Virt = virt;
  extentStreamSpec->Extents.Add(se);
  extentStreamSpec->Stream = _stream;
  extentStreamSpec->Init();
  *stream = extentStream.Detach();
  
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
    totalSize += _items[index].GetSize();
  }
  extractCallback->SetTotal(totalSize);

  totalSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = totalSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    CMyComPtr<ISequentialOutStream> outStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    
    RINOK(extractCallback->GetStream(index, &outStream, askMode))

    const UInt64 size = _items[index].GetSize();
    totalSize += size;
    if (!testMode && !outStream)
      continue;
    
    RINOK(extractCallback->PrepareOperation(askMode))

    CMyComPtr<ISequentialInStream> inStream;
    const HRESULT hres = GetStream(index, &inStream);
    int opRes = NExtract::NOperationResult::kUnsupportedMethod;
    if (hres != S_FALSE)
    {
      if (hres != S_OK)
        return hres;
      RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress))
      opRes = NExtract::NOperationResult::kDataError;
      if (copyCoderSpec->TotalSize == size)
        opRes = NExtract::NOperationResult::kOK;
      else if (copyCoderSpec->TotalSize < size)
        opRes = NExtract::NOperationResult::kUnexpectedEnd;
    }
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  
  return S_OK;
  COM_TRY_END
}


REGISTER_ARC_I(
  "LP", "lpimg img", NULL, 0xc1,
  k_Signature,
  LP_PARTITION_RESERVED_BYTES,
  0,
  NULL)

}}
