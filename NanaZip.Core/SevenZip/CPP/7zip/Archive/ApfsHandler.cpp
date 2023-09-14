// ApfsHandler.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif

#include "../../../C/CpuArch.h"
#include "../../../C/Sha256.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer2.h"
#include "../../Common/MyLinux.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariantConv.h"
#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/ItemNameUtils.h"

#include "HfsHandler.h"

// if APFS_SHOW_ALT_STREAMS is defined, the handler will show attribute files.
#define APFS_SHOW_ALT_STREAMS

#define VI_MINUS1   ((unsigned)(int)-1)
#define IsViDef(x)      ((int)(x) != -1)
#define IsViNotDef(x)   ((int)(x) == -1)

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define G16(_offs_, dest) dest = Get16(p + (_offs_));
#define G32(_offs_, dest) dest = Get32(p + (_offs_));
#define G64(_offs_, dest) dest = Get64(p + (_offs_));

namespace NArchive {
namespace NApfs {

#define ValToHex(t) ((char)(((t) < 10) ? ('0' + (t)) : ('a' + ((t) - 10))))

static void ConvertByteToHex(unsigned val, char *s)
{
  unsigned t;
  t = val >> 4;
  s[0] = ValToHex(t);
  t = val & 0xF;
  s[1] = ValToHex(t);
}

struct CUuid
{
  Byte Data[16];

  void SetHex_To_str(char *s) const
  {
    for (unsigned i = 0; i < 16; i++)
      ConvertByteToHex(Data[i], s + i * 2);
    s[32] = 0;
  }

  void AddHexToString(UString &dest) const
  {
    char temp[32 + 4];
    SetHex_To_str(temp);
    dest += temp;
  }

  void SetFrom(const Byte *p) { memcpy(Data, p, 16); }
};


typedef UInt64  oid_t;
typedef UInt64  xid_t;
// typedef Int64   paddr_t;
typedef UInt64   paddr_t_unsigned;

#define G64o G64
#define G64x G64
// #define G64a G64

/*
struct prange_t
{
  paddr_t start_paddr;
  UInt64 block_count;
  
  void Parse(const Byte *p)
  {
    G64a (0, start_paddr);
    G64 (8, block_count);
  }
};
*/

#define OBJECT_TYPE_NX_SUPERBLOCK       0x1
#define OBJECT_TYPE_BTREE               0x2
#define OBJECT_TYPE_BTREE_NODE          0x3
/*
#define OBJECT_TYPE_SPACEMAN            0x5
#define OBJECT_TYPE_SPACEMAN_CAB        0x6
#define OBJECT_TYPE_SPACEMAN_CIB        0x7
#define OBJECT_TYPE_SPACEMAN_BITMAP     0x8
#define OBJECT_TYPE_SPACEMAN_FREE_QUEUE 0x9
#define OBJECT_TYPE_EXTENT_LIST_TREE    0xa
*/
#define OBJECT_TYPE_OMAP                0xb
/*
#define OBJECT_TYPE_CHECKPOINT_MAP      0xc
*/
#define OBJECT_TYPE_FS                  0xd
#define OBJECT_TYPE_FSTREE              0xe
/*
#define OBJECT_TYPE_BLOCKREFTREE        0xf
#define OBJECT_TYPE_SNAPMETATREE        0x10
#define OBJECT_TYPE_NX_REAPER           0x11
#define OBJECT_TYPE_NX_REAP_LIST        0x12
#define OBJECT_TYPE_OMAP_SNAPSHOT       0x13
#define OBJECT_TYPE_EFI_JUMPSTART       0x14
#define OBJECT_TYPE_FUSION_MIDDLE_TREE  0x15
#define OBJECT_TYPE_NX_FUSION_WBC       0x16
#define OBJECT_TYPE_NX_FUSION_WBC_LIST  0x17
#define OBJECT_TYPE_ER_STATE            0x18
#define OBJECT_TYPE_GBITMAP             0x19
#define OBJECT_TYPE_GBITMAP_TREE        0x1a
#define OBJECT_TYPE_GBITMAP_BLOCK       0x1b
#define OBJECT_TYPE_ER_RECOVERY_BLOCK   0x1c
#define OBJECT_TYPE_SNAP_META_EXT       0x1d
*/
#define OBJECT_TYPE_INTEGRITY_META      0x1e
#define OBJECT_TYPE_FEXT_TREE           0x1f
/*
#define OBJECT_TYPE_RESERVED_20         0x20

#define OBJECT_TYPE_INVALID       0x0
#define OBJECT_TYPE_TEST          0xff
#define OBJECT_TYPE_CONTAINER_KEYBAG  'keys'
#define OBJECT_TYPE_VOLUME_KEYBAG     'recs'
#define OBJECT_TYPE_MEDIA_KEYBAG      'mkey'

#define OBJ_VIRTUAL       0x0
#define OBJ_EPHEMERAL     0x80000000
*/
#define OBJ_PHYSICAL      0x40000000
/*
#define OBJ_NOHEADER      0x20000000
#define OBJ_ENCRYPTED     0x10000000
#define OBJ_NONPERSISTENT 0x08000000
*/
#define OBJECT_TYPE_MASK                0x0000ffff
/*
#define OBJECT_TYPE_FLAGS_MASK          0xffff0000
#define OBJ_STORAGETYPE_MASK            0xc0000000
#define OBJECT_TYPE_FLAGS_DEFINED_MASK  0xf8000000
*/

// #define MAX_CKSUM_SIZE 8

static const unsigned k_obj_phys_Size = 0x20;

// obj_phys_t
struct CPhys
{
  // Byte cksum[MAX_CKSUM_SIZE];
  oid_t oid;
  xid_t xid;
  UInt32 type;
  UInt32 subtype;

  UInt32 GetType() const { return type & OBJECT_TYPE_MASK; }
  void Parse(const Byte *p);
};

void CPhys::Parse(const Byte *p)
{
  // memcpy(cksum, p, MAX_CKSUM_SIZE);
  G64o (8, oid)
  G64x (0x10, xid)
  G32 (0x18, type)
  G32 (0x1C, subtype)
}

#define NX_MAX_FILE_SYSTEMS   100
/*
#define NX_EPH_INFO_COUNT   4
#define NX_EPH_MIN_BLOCK_COUNT  8
#define NX_MAX_FILE_SYSTEM_EPH_STRUCTS  4
#define NX_TX_MIN_CHECKPOINT_COUNT  4
#define NX_EPH_INFO_VERSION_1   1
*/

/*
typedef enum
{
  NX_CNTR_OBJ_CKSUM_SET = 0,
  NX_CNTR_OBJ_CKSUM_FAIL = 1,
  NX_NUM_COUNTERS = 32
} counter_id_t;
*/

/* Incompatible volume feature flags */
#define APFS_INCOMPAT_CASE_INSENSITIVE          (1 << 0)
/*
#define APFS_INCOMPAT_DATALESS_SNAPS            (1 << 1)
#define APFS_INCOMPAT_ENC_ROLLED                (1 << 2)
*/
#define APFS_INCOMPAT_NORMALIZATION_INSENSITIVE (1 << 3)
/*
#define APFS_INCOMPAT_INCOMPLETE_RESTORE        (1 << 4)
*/
#define APFS_INCOMPAT_SEALED_VOLUME             (1 << 5)
/*
#define APFS_INCOMPAT_RESERVED_40               (1 << 6)
*/

static const char * const g_APFS_INCOMPAT_Flags[] =
{
    "CASE_INSENSITIVE"
  , "DATALESS_SNAPS"
  , "ENC_ROLLED"
  , "NORMALIZATION_INSENSITIVE"
  , "INCOMPLETE_RESTORE"
  , "SEALED_VOLUME"
};

/*
#define APFS_SUPPORTED_INCOMPAT_MASK \
    ( APFS_INCOMPAT_CASE_INSENSITIVE \
    | APFS_INCOMPAT_DATALESS_SNAPS \
    | APFS_INCOMPAT_ENC_ROLLED \
    | APFS_INCOMPAT_NORMALIZATION_INSENSITIVE \
    | APFS_INCOMPAT_INCOMPLETE_RESTORE \
    | APFS_INCOMPAT_SEALED_VOLUME \
    | APFS_INCOMPAT_RESERVED_40 \
)
*/

// superblock_t
struct CSuperBlock
{
  // CPhys o;
  // UInt32 magic;
  UInt32 block_size;
  unsigned block_size_Log;
  UInt64 block_count;
  // UInt64 features;
  // UInt64 readonly_compatible_features;
  // UInt64 incompatible_features;
  CUuid uuid;
  /*
  oid_t next_oid;
  xid_t next_xid;
  UInt32 xp_desc_blocks;
  UInt32 xp_data_blocks;
  paddr_t xp_desc_base;
  paddr_t xp_data_base;
  UInt32 xp_desc_next;
  UInt32 xp_data_next;
  UInt32 xp_desc_index;
  UInt32 xp_desc_len;
  UInt32 xp_data_index;
  UInt32 xp_data_len;
  oid_t spaceman_oid;
  */
  oid_t omap_oid;
  // oid_t reaper_oid;
  // UInt32 test_type;
  UInt32 max_file_systems;
  // oid_t fs_oid[NX_MAX_FILE_SYSTEMS];
  /*
  UInt64 counters[NX_NUM_COUNTERS]; // counter_id_t
  prange_t blocked_out_prange;
  oid_t evict_mapping_tree_oid;
  UInt64 flags;
  paddr_t efi_jumpstart;
  CUuid fusion_uuid;
  prange_t keylocker;
  UInt64 ephemeral_info[NX_EPH_INFO_COUNT];
  oid_t test_oid;
  oid_t fusion_mt_oid;
  oid_t fusion_wbc_oid;
  prange_t fusion_wbc;
  UInt64 newest_mounted_version;
  prange_t mkb_locker;
  */

  bool Parse(const Byte *p);
};

struct CSuperBlock2
{
  oid_t fs_oid[NX_MAX_FILE_SYSTEMS];
  void Parse(const Byte *p)
  {
    for (unsigned i = 0; i < NX_MAX_FILE_SYSTEMS; i++)
    {
      G64o (0xb8 + i * 8, fs_oid[i])
    }
  }
};


// we include one additional byte of next field (block_size)
static const unsigned k_SignatureOffset = 32;
static const Byte k_Signature[] = { 'N', 'X', 'S', 'B', 0 };

// size must be 4 bytes aligned
static UInt64 Fletcher64(const Byte *data, size_t size)
{
  const UInt32 kMax32 = 0xffffffff;
  const UInt64 val = 0; // startVal
  UInt64 a = val & kMax32;
  UInt64 b = (val >> 32) & kMax32;
  for (size_t i = 0; i < size; i += 4)
  {
    a += GetUi32(data + i);
    b += a;
  }
  a %= kMax32;
  b %= kMax32;
  b = (UInt32)(kMax32 - ((a + b) % kMax32));
  a = (UInt32)(kMax32 - ((a + b) % kMax32));
  return (a << 32) | b;
}

static bool CheckFletcher64(const Byte *p, size_t size)
{
  const UInt64 calculated_checksum = Fletcher64(p + 8, size - 8);
  const UInt64 stored_checksum = Get64(p);
  return (stored_checksum == calculated_checksum);
}


static unsigned GetLogSize(UInt32 size)
{
  unsigned k;
  for (k = 0; k < 32; k++)
    if (((UInt32)1 << k) == size)
      return k;
  return k;
}

static const unsigned kApfsHeaderSize = 1 << 12;

// #define OID_INVALID         0
#define OID_NX_SUPERBLOCK   1
// #define OID_RESERVED_COUNT  1024
// This range of identifiers is reserved for physical, virtual, and ephemeral objects

bool CSuperBlock::Parse(const Byte *p)
{
  CPhys o;
  o.Parse(p);
  if (o.oid != OID_NX_SUPERBLOCK)
    return false;
  if (o.GetType() != OBJECT_TYPE_NX_SUPERBLOCK)
    return false;
  if (o.subtype != 0)
    return false;
  if (memcmp(p + k_SignatureOffset, k_Signature, 4) != 0)
    return false;
  if (!CheckFletcher64(p, kApfsHeaderSize))
    return false;

  G32 (0x24, block_size)
  {
    const unsigned logSize = GetLogSize(block_size);
    // don't change it. Some code requires (block_size <= 16)
    if (logSize < 12 || logSize > 16)
      return false;
    block_size_Log = logSize;
  }

  G64 (0x28, block_count)

  const UInt64 kArcSize_MAX = (UInt64)1 << 62;
  if (block_count > (kArcSize_MAX >> block_size_Log))
    return false;

  // G64 (0x30, features);
  // G64 (0x38, readonly_compatible_features);
  // G64 (0x40, incompatible_features);
  uuid.SetFrom(p + 0x48);
  /*
  G64o (0x58, next_oid);
  G64x (0x60, next_xid);
  G32 (0x68, xp_desc_blocks);
  G32 (0x6c, xp_data_blocks);
  G64a (0x70, xp_desc_base);
  G64a (0x78, xp_data_base);
  G32 (0x80, xp_desc_next);
  G32 (0x84, xp_data_next);
  G32 (0x88, xp_desc_index);
  G32 (0x8c, xp_desc_len);
  G32 (0x90, xp_data_index);
  G32 (0x94, xp_data_len);
  G64o (0x98, spaceman_oid);
  */
  G64o (0xa0, omap_oid)
  // G64o (0xa8, reaper_oid);
  // G32 (0xb0, test_type);
  G32 (0xb4, max_file_systems)
  if (max_file_systems > NX_MAX_FILE_SYSTEMS)
    return false;
  /*
  {
    for (unsigned i = 0; i < NX_MAX_FILE_SYSTEMS; i++)
    {
      G64o (0xb8 + i * 8, fs_oid[i]);
    }
  }
  */
  /*
  {
    for (unsigned i = 0; i < NX_NUM_COUNTERS; i++)
    {
      G64 (0x3d8 + i * 8, counters[i]);
    }
  }
  blocked_out_prange.Parse(p + 0x4d8);
  G64o (0x4e8, evict_mapping_tree_oid);
  #define NX_CRYPTO_SW 0x00000004LL
  G64 (0x4f0, flags);
  G64a (0x4f8, efi_jumpstart);
  fusion_uuid.SetFrom(p + 0x500);
  keylocker.Parse(p + 0x510);
  {
    for (unsigned i = 0; i < NX_EPH_INFO_COUNT; i++)
    {
      G64 (0x520 + i * 8, ephemeral_info[i]);
    }
  }
  G64o (0x540, test_oid);
  G64o (0x548, fusion_mt_oid);
  G64o (0x550, fusion_wbc_oid);
  fusion_wbc.Parse(p + 0x558);
  G64 (0x568, newest_mounted_version); // decimal 1412141 001 000 000
  mkb_locker.Parse(p + 0x570);
  */

  return true;
}


enum apfs_hash_type_t
{
  APFS_HASH_INVALID = 0,
  APFS_HASH_SHA256 = 1,
  APFS_HASH_SHA512_256 = 2,
  APFS_HASH_SHA384 = 3,
  APFS_HASH_SHA512 = 4,

  APFS_HASH_MIN = APFS_HASH_SHA256,
  APFS_HASH_MAX = APFS_HASH_SHA512,

  APFS_HASH_DEFAULT = APFS_HASH_SHA256
};

static unsigned GetHashSize(unsigned hashType)
{
  if (hashType > APFS_HASH_MAX) return 0;
  if (hashType < APFS_HASH_MIN) return 0;
  if (hashType == APFS_HASH_SHA256) return 32;
  return hashType * 16;
}

#define APFS_HASH_MAX_SIZE 64

static const char * const g_hash_types[] =
{
    NULL
  , "SHA256"
  , "SHA512_256"
  , "SHA384"
  , "SHA512"
};


struct C_integrity_meta_phys
{
  // CPhys im_o;
  // UInt32 im_version;
  UInt32 im_flags;
  // apfs_hash_type_t
  UInt32 im_hash_type;
  // UInt32 im_root_hash_offset;
  // xid_t im_broken_xid;
  // UInt64 im_reserved[9];

  unsigned HashSize;
  Byte Hash[APFS_HASH_MAX_SIZE];

  bool Is_SHA256() const { return im_hash_type == APFS_HASH_SHA256; }

  C_integrity_meta_phys()
  {
    memset(this, 0, sizeof(*this));
  }
  bool Parse(const Byte *p, size_t size, oid_t oid);
};

bool C_integrity_meta_phys::Parse(const Byte *p, size_t size, oid_t oid)
{
  CPhys o;
  if (!CheckFletcher64(p, size))
    return false;
  o.Parse(p);
  if (o.GetType() != OBJECT_TYPE_INTEGRITY_META)
    return false;
  if (o.oid != oid)
    return false;
  // G32 (0x20, im_version);
  G32 (0x24, im_flags)
  G32 (0x28, im_hash_type)
  UInt32 im_root_hash_offset;
  G32 (0x2C, im_root_hash_offset)
  // G64x (0x30, im_broken_xid);
  const unsigned hashSize = GetHashSize(im_hash_type);
  HashSize = hashSize;
  if (im_root_hash_offset >= size || size - im_root_hash_offset < hashSize)
    return false;
  memcpy(Hash, p + im_root_hash_offset, hashSize);
  return true;
}


struct C_omap_phys
{
  // om_ prefix
  // CPhys o;
  /*
  UInt32 flags;
  UInt32 snap_count;
  UInt32 tree_type;
  UInt32 snapshot_tree_type;
  */
  oid_t tree_oid;
  /*
  oid_t snapshot_tree_oid;
  xid_t most_recent_snap;
  xid_t pending_revert_min;
  xid_t pending_revert_max;
  */
  bool Parse(const Byte *p, size_t size, oid_t oid);
};

bool C_omap_phys::Parse(const Byte *p, size_t size, oid_t oid)
{
  CPhys o;
  if (!CheckFletcher64(p, size))
    return false;
  o.Parse(p);
  if (o.GetType() != OBJECT_TYPE_OMAP)
    return false;
  if (o.oid != oid)
    return false;
  /*
  G32 (0x20, flags);
  G32 (0x24, snap_count);
  G32 (0x28, tree_type);
  G32 (0x2C, snapshot_tree_type);
  */
  G64o (0x30, tree_oid)
  /*
  G64o (0x38, snapshot_tree_oid);
  G64x (0x40, most_recent_snap);
  G64x (0x48, pending_revert_min);
  G64x (0x50, pending_revert_max);
  */
  return true;
}


// #define BTOFF_INVALID 0xffff
/* This value is stored in the off field of nloc_t to indicate that
there's no offset. For example, the last entry in a free
list has no entry after it, so it uses this value for its off field. */

// A location within a B-tree node
struct nloc
{
  UInt16 off;
  UInt16 len;
  
  void Parse(const Byte *p)
  {
    G16 (0, off)
    G16 (2, len)
  }
  UInt32 GetEnd() const { return (UInt32)off + len; }
  bool CheckOverLimit(UInt32 limit)
  {
    return off < limit && len <= limit - off;
  }
};


// The location, within a B-tree node, of a key and value
struct kvloc
{
  nloc k;
  nloc v;
  
  void Parse(const Byte *p)
  {
    k.Parse(p);
    v.Parse(p + 4);
  }
};


// The location, within a B-tree node, of a fixed-size key and value
struct kvoff
{
  UInt16 k;
  UInt16 v;

  void Parse(const Byte *p)
  {
    G16 (0, k)
    G16 (2, v)
  }
};


#define BTNODE_ROOT             (1 << 0)
#define BTNODE_LEAF             (1 << 1)
#define BTNODE_FIXED_KV_SIZE    (1 << 2)
#define BTNODE_HASHED           (1 << 3)
#define BTNODE_NOHEADER         (1 << 4)
/*
#define BTNODE_CHECK_KOFF_INVAL (1 << 15)
*/

static const unsigned k_Toc_offset = 0x38;

// btree_node_phys
struct CBTreeNodePhys
{
  // btn_ prefix
  CPhys ophys;
  UInt16 flags;
  UInt16 level;       // the number of child levels below this node. 0 -  for a leaf node, 1 for the immediate parent of a leaf node
  UInt32 nkeys;       // The number of keys stored in this node.
  nloc table_space;
  /*
  nloc free_space;
  nloc key_free_list;
  nloc val_free_list;
  */

  bool Is_FIXED_KV_SIZE() const { return (flags & BTNODE_FIXED_KV_SIZE) != 0; }
  bool Is_NOHEADER() const { return (flags & BTNODE_NOHEADER) != 0; }
  bool Is_HASHED() const { return (flags & BTNODE_HASHED) != 0; }

  bool Parse(const Byte *p, size_t size, bool noHeader = false)
  {
    G16 (0x20, flags)
    G16 (0x22, level)
    G32 (0x24, nkeys)
    table_space.Parse(p + 0x28);
    /*
    free_space.Parse(p + 0x2C);
    key_free_list.Parse(p + 0x30);
    val_free_list.Parse(p + 0x34);
    */
    memset(&ophys, 0, sizeof(ophys));
    if (noHeader)
    {
      for (unsigned i = 0; i < k_obj_phys_Size; i++)
        if (p[i] != 0)
          return false;
    }
    else
    {
      if (!CheckFletcher64(p, size))
        return false;
      ophys.Parse(p);
    }
    if (Is_NOHEADER() != noHeader)
      return false;
    return true;
  }
};

/*
#define BTREE_UINT64_KEYS       (1 << 0)
#define BTREE_SEQUENTIAL_INSERT (1 << 1)
#define BTREE_ALLOW_GHOSTS      (1 << 2)
*/
#define BTREE_EPHEMERAL         (1 << 3)
#define BTREE_PHYSICAL          (1 << 4)
/*
#define BTREE_NONPERSISTENT     (1 << 5)
#define BTREE_KV_NONALIGNED     (1 << 6)
#define BTREE_HASHED            (1 << 7)
*/
#define BTREE_NOHEADER          (1 << 8)

/*
  BTREE_EPHEMERAL: The nodes in the B-tree use ephemeral object identifiers to link to child nodes
  BTREE_PHYSICAL : The nodes in the B-tree use physical object identifiers to link to child nodes.
  If neither flag is set, nodes in the B-tree use virtual object
  identifiers to link to their child nodes.
*/

// Static information about a B-tree.
struct btree_info_fixed
{
  UInt32 flags;
  UInt32 node_size;
  UInt32 key_size;
  UInt32 val_size;

  void Parse(const Byte *p)
  {
    G32 (0, flags)
    G32 (4, node_size)
    G32 (8, key_size)
    G32 (12, val_size)
  }
};

static const unsigned k_btree_info_Size = 0x28;

struct btree_info
{
  btree_info_fixed fixed;
  UInt32 longest_key;
  UInt32 longest_val;
  UInt64 key_count;
  UInt64 node_count;

  bool Is_EPHEMERAL() const { return (fixed.flags & BTREE_EPHEMERAL) != 0; }
  bool Is_PHYSICAL()  const { return (fixed.flags & BTREE_PHYSICAL) != 0; }
  bool Is_NOHEADER()  const { return (fixed.flags & BTREE_NOHEADER) != 0; }

  void Parse(const Byte *p)
  {
    fixed.Parse(p);
    G32 (0x10, longest_key)
    G32 (0x14, longest_val)
    G64 (0x18, key_count)
    G64 (0x20, node_count)
  }
};


/*
typedef UInt32  cp_key_class_t;
typedef UInt32  cp_key_os_version_t;
typedef UInt16  cp_key_revision_t;
typedef UInt32  crypto_flags_t;

struct wrapped_meta_crypto_state
{
  UInt16 major_version;
  UInt16 minor_version;
  crypto_flags_t cpflags;
  cp_key_class_t persistent_class;
  cp_key_os_version_t key_os_version;
  cp_key_revision_t key_revision;
  // UInt16 unused;

  void Parse(const Byte *p)
  {
    G16 (0, major_version);
    G16 (2, minor_version);
    G32 (4, cpflags);
    G32 (8, persistent_class);
    G32 (12, key_os_version);
    G16 (16, key_revision);
  }
};
*/


#define APFS_MODIFIED_NAMELEN 32
#define sizeof_apfs_modified_by_t (APFS_MODIFIED_NAMELEN + 16)

struct apfs_modified_by_t
{
  Byte id[APFS_MODIFIED_NAMELEN];
  UInt64 timestamp;
  xid_t last_xid;
  
  void Parse(const Byte *p)
  {
    memcpy(id, p, APFS_MODIFIED_NAMELEN);
    p += APFS_MODIFIED_NAMELEN;
    G64 (0, timestamp)
    G64x (8, last_xid)
  }
};


#define APFS_MAX_HIST 8
#define APFS_VOLNAME_LEN 256

struct CApfs
{
  // apfs_
  CPhys o;
  // UInt32 magic;
  UInt32 fs_index; // index of the object identifier for this volume's file system in the container's array of file systems.
  // UInt64 features;
  // UInt64 readonly_compatible_features;
  UInt64 incompatible_features;
  UInt64 unmount_time;
  // UInt64 fs_reserve_block_count;
  // UInt64 fs_quota_block_count;
  UInt64 fs_alloc_count;
  // wrapped_meta_crypto_state meta_crypto;
  // UInt32 root_tree_type;
    /* The type of the root file-system tree.
        The value is typically OBJ_VIRTUAL | OBJECT_TYPE_BTREE,
        with a subtype of OBJECT_TYPE_FSTREE */

  // UInt32 extentref_tree_type;
  // UInt32 snap_meta_tree_type;
  oid_t omap_oid;
  oid_t root_tree_oid;
  /*
  oid_t extentref_tree_oid;
  oid_t snap_meta_tree_oid;
  xid_t revert_to_xid;
  oid_t revert_to_sblock_oid;
  UInt64 next_obj_id;
  */
  UInt64 num_files;
  UInt64 num_directories;
  UInt64 num_symlinks;
  UInt64 num_other_fsobjects;
  UInt64 num_snapshots;
  UInt64 total_blocks_alloced;
  UInt64 total_blocks_freed;
  CUuid vol_uuid;
  UInt64 last_mod_time;
  UInt64 fs_flags;
  apfs_modified_by_t formatted_by;
  apfs_modified_by_t modified_by[APFS_MAX_HIST];
  Byte volname[APFS_VOLNAME_LEN];
  /*
  UInt32 next_doc_id;
  UInt16 role; //  APFS_VOL_ROLE_NONE   APFS_VOL_ROLE_SYSTEM ....
  UInt16 reserved;
  xid_t root_to_xid;
  oid_t er_state_oid;
  UInt64 cloneinfo_id_epoch;
  UInt64 cloneinfo_xid;
  oid_t snap_meta_ext_oid;
  CUuid volume_group_id;
  */
  oid_t integrity_meta_oid; // virtual object identifier of the integrity metadata object
  oid_t fext_tree_oid;      // virtual object identifier of the file extent tree.
  UInt32 fext_tree_type;    // The type of the file extent tree.
  /*
  UInt32 reserved_type;
  oid_t reserved_oid;
  */

  UInt64 GetTotalItems() const
  {
    return num_files + num_directories + num_symlinks + num_other_fsobjects;
  }

  bool IsHashedName() const
  {
    return
      (incompatible_features & APFS_INCOMPAT_CASE_INSENSITIVE) != 0 ||
      (incompatible_features & APFS_INCOMPAT_NORMALIZATION_INSENSITIVE) != 0;
  }

  bool Parse(const Byte *p, size_t size);
};


bool CApfs::Parse(const Byte *p, size_t size)
{
  o.Parse(p);
  if (Get32(p + 32) != 0x42535041) //  { 'A', 'P', 'S', 'B' };
    return false;
  if (o.GetType() != OBJECT_TYPE_FS)
    return false;
  if (!CheckFletcher64(p, size))
    return false;
  // if (o.GetType() != OBJECT_TYPE_NX_SUPERBLOCK) return false;

  G32 (0x24, fs_index)
  // G64 (0x28, features);
  // G64 (0x30, readonly_compatible_features);
  G64 (0x38, incompatible_features)
  G64 (0x40, unmount_time)
  // G64 (0x48, fs_reserve_block_count);
  // G64 (0x50, fs_quota_block_count);
  G64 (0x58, fs_alloc_count)
  // meta_crypto.Parse(p + 0x60);
  // G32 (0x74, root_tree_type);
  // G32 (0x78, extentref_tree_type);
  // G32 (0x7C, snap_meta_tree_type);

  G64o (0x80, omap_oid)
  G64o (0x88, root_tree_oid)
  /*
  G64o (0x90, extentref_tree_oid);
  G64o (0x98, snap_meta_tree_oid);
  G64x (0xa0, revert_to_xid);
  G64o (0xa8, revert_to_sblock_oid);
  G64 (0xb0, next_obj_id);
  */
  G64 (0xb8, num_files)
  G64 (0xc0, num_directories)
  G64 (0xc8, num_symlinks)
  G64 (0xd0, num_other_fsobjects)
  G64 (0xd8, num_snapshots)
  G64 (0xe0, total_blocks_alloced)
  G64 (0xe8, total_blocks_freed)
  vol_uuid.SetFrom(p + 0xf0);
  G64 (0x100, last_mod_time)
  G64 (0x108, fs_flags)
  p += 0x110;
  formatted_by.Parse(p);
  p += sizeof_apfs_modified_by_t;
  for (unsigned i = 0; i < APFS_MAX_HIST; i++)
  {
    modified_by[i].Parse(p);
    p += sizeof_apfs_modified_by_t;
  }
  memcpy(volname, p, APFS_VOLNAME_LEN);
  p += APFS_VOLNAME_LEN;
  /*
  G32 (0, next_doc_id);
  G16 (4, role);
  G16 (6, reserved);
  G64x (8, root_to_xid);
  G64o (0x10, er_state_oid);
  G64 (0x18, cloneinfo_id_epoch);
  G64 (0x20, cloneinfo_xid);
  G64o (0x28, snap_meta_ext_oid);
  volume_group_id.SetFrom(p + 0x30);
  */
  G64o (0x40, integrity_meta_oid)
  G64o (0x48, fext_tree_oid)
  G32 (0x50, fext_tree_type)
  /*
  G32 (0x54, reserved_type);
  G64o (0x58, reserved_oid);
  */
  return true;
}


#define OBJ_ID_MASK         0x0fffffffffffffff
/*
#define OBJ_TYPE_MASK       0xf000000000000000
#define SYSTEM_OBJ_ID_MARK  0x0fffffff00000000
*/
#define OBJ_TYPE_SHIFT      60

typedef enum
{
  APFS_TYPE_ANY           = 0,
  APFS_TYPE_SNAP_METADATA = 1,
  APFS_TYPE_EXTENT        = 2,
  APFS_TYPE_INODE         = 3,
  APFS_TYPE_XATTR         = 4,
  APFS_TYPE_SIBLING_LINK  = 5,
  APFS_TYPE_DSTREAM_ID    = 6,
  APFS_TYPE_CRYPTO_STATE  = 7,
  APFS_TYPE_FILE_EXTENT   = 8,
  APFS_TYPE_DIR_REC       = 9,
  APFS_TYPE_DIR_STATS     = 10,
  APFS_TYPE_SNAP_NAME     = 11,
  APFS_TYPE_SIBLING_MAP   = 12,
  APFS_TYPE_FILE_INFO     = 13,
  APFS_TYPE_MAX_VALID     = 13,
  APFS_TYPE_MAX           = 15,
  APFS_TYPE_INVALID       = 15
} j_obj_types;


struct j_key_t
{
  UInt64 obj_id_and_type;

  void Parse(const Byte *p) { G64(0, obj_id_and_type) }
  unsigned GetType() const { return (unsigned)(obj_id_and_type >> OBJ_TYPE_SHIFT); }
  UInt64 GetID() const { return obj_id_and_type & OBJ_ID_MASK; }
};



#define J_DREC_LEN_MASK   0x000003ff
/*
#define J_DREC_HASH_MASK  0xfffff400
#define J_DREC_HASH_SHIFT 10
*/

static const unsigned k_SizeOf_j_drec_val = 0x12;

struct j_drec_val
{
  UInt64 file_id;
  UInt64 date_added; /* The time that this directory entry was added to the directory.
       It's not updated when modifying the directory entry for example,
       by renaming a file without moving it to a different directory. */
  UInt16 flags;

  // bool IsFlags_File() const { return flags == MY_LIN_DT_REG; }
  bool IsFlags_Unknown() const { return flags == MY_LIN_DT_UNKNOWN; }
  bool IsFlags_Dir()     const { return flags == MY_LIN_DT_DIR; }

  // uint8_t xfields[];
  void Parse(const Byte *p)
  {
    G64 (0, file_id)
    G64 (8, date_added)
    G16 (0x10, flags)
  }
};


struct CItem
{
  // j_key_t hdr;
  UInt64 ParentId;
  AString Name;
  j_drec_val Val;

  unsigned ParentItemIndex;
  unsigned RefIndex;
  // unsigned  iNode_Index;
  
  void Clear()
  {
    Name.Empty();
    ParentItemIndex = VI_MINUS1;
    RefIndex = VI_MINUS1;
  }
};


/*
#define INVALID_INO_NUM       0
#define ROOT_DIR_PARENT       1  // parent for "root" and "private-dir", there's no inode on disk with this inode number.
*/
#define ROOT_DIR_INO_NUM      2  // "root" - parent for all main files
#define PRIV_DIR_INO_NUM      3  // "private-dir"
/*
#define SNAP_DIR_INO_NUM      6  //  the directory where snapshot metadata is stored. Snapshot inodes are stored in the snapshot metedata tree.
#define PURGEABLE_DIR_INO_NUM 7
#define MIN_USER_INO_NUM      16

#define UNIFIED_ID_SPACE_MARK 0x0800000000000000
*/

//typedef enum
// {
/*
INODE_IS_APFS_PRIVATE         = 0x00000001,
INODE_MAINTAIN_DIR_STATS      = 0x00000002,
INODE_DIR_STATS_ORIGIN        = 0x00000004,
INODE_PROT_CLASS_EXPLICIT     = 0x00000008,
INODE_WAS_CLONED              = 0x00000010,
INODE_FLAG_UNUSED             = 0x00000020,
INODE_HAS_SECURITY_EA         = 0x00000040,
INODE_BEING_TRUNCATED         = 0x00000080,
INODE_HAS_FINDER_INFO         = 0x00000100,
INODE_IS_SPARSE               = 0x00000200,
INODE_WAS_EVER_CLONED         = 0x00000400,
INODE_ACTIVE_FILE_TRIMMED     = 0x00000800,
INODE_PINNED_TO_MAIN          = 0x00001000,
INODE_PINNED_TO_TIER2         = 0x00002000,
*/
// INODE_HAS_RSRC_FORK           = 0x00004000,
/*
INODE_NO_RSRC_FORK            = 0x00008000,
INODE_ALLOCATION_SPILLEDOVER  = 0x00010000,
INODE_FAST_PROMOTE            = 0x00020000,
*/
#define INODE_HAS_UNCOMPRESSED_SIZE  0x00040000
/*
INODE_IS_PURGEABLE            = 0x00080000,
INODE_WANTS_TO_BE_PURGEABLE   = 0x00100000,
INODE_IS_SYNC_ROOT            = 0x00200000,
INODE_SNAPSHOT_COW_EXEMPTION  = 0x00400000,


INODE_INHERITED_INTERNAL_FLAGS = \
    ( INODE_MAINTAIN_DIR_STATS \
    | INODE_SNAPSHOT_COW_EXEMPTION),

INODE_CLONED_INTERNAL_FLAGS = \
    ( INODE_HAS_RSRC_FORK \
    | INODE_NO_RSRC_FORK \
    | INODE_HAS_FINDER_INFO \
    | INODE_SNAPSHOT_COW_EXEMPTION),
*/
// }
// j_inode_flags;


/*
#define APFS_VALID_INTERNAL_INODE_FLAGS \
( INODE_IS_APFS_PRIVATE \
| INODE_MAINTAIN_DIR_STATS \
| INODE_DIR_STATS_ORIGIN \
| INODE_PROT_CLASS_EXPLICIT \
| INODE_WAS_CLONED \
| INODE_HAS_SECURITY_EA \
| INODE_BEING_TRUNCATED \
| INODE_HAS_FINDER_INFO \
| INODE_IS_SPARSE \
| INODE_WAS_EVER_CLONED \
| INODE_ACTIVE_FILE_TRIMMED \
| INODE_PINNED_TO_MAIN \
| INODE_PINNED_TO_TIER2 \
| INODE_HAS_RSRC_FORK \
| INODE_NO_RSRC_FORK \
| INODE_ALLOCATION_SPILLEDOVER \
| INODE_FAST_PROMOTE \
| INODE_HAS_UNCOMPRESSED_SIZE \
| INODE_IS_PURGEABLE \
| INODE_WANTS_TO_BE_PURGEABLE \
| INODE_IS_SYNC_ROOT \
| INODE_SNAPSHOT_COW_EXEMPTION)

#define APFS_INODE_PINNED_MASK (INODE_PINNED_TO_MAIN | INODE_PINNED_TO_TIER2)
*/

static const char * const g_INODE_Flags[] =
{
    "IS_APFS_PRIVATE"
  , "MAINTAIN_DIR_STATS"
  , "DIR_STATS_ORIGIN"
  , "PROT_CLASS_EXPLICIT"
  , "WAS_CLONED"
  , "FLAG_UNUSED"
  , "HAS_SECURITY_EA"
  , "BEING_TRUNCATED"
  , "HAS_FINDER_INFO"
  , "IS_SPARSE"
  , "WAS_EVER_CLONED"
  , "ACTIVE_FILE_TRIMMED"
  , "PINNED_TO_MAIN"
  , "PINNED_TO_TIER2"
  , "HAS_RSRC_FORK"
  , "NO_RSRC_FORK"
  , "ALLOCATION_SPILLEDOVER"
  , "FAST_PROMOTE"
  , "HAS_UNCOMPRESSED_SIZE"
  , "IS_PURGEABLE"
  , "WANTS_TO_BE_PURGEABLE"
  , "IS_SYNC_ROOT"
  , "SNAPSHOT_COW_EXEMPTION"
};


// bsd stat.h
/*
#define MY_UF_SETTABLE   0x0000ffff  // mask of owner changeable flags
#define MY_UF_NODUMP     0x00000001  // do not dump file
#define MY_UF_IMMUTABLE  0x00000002  // file may not be changed
#define MY_UF_APPEND     0x00000004  // writes to file may only append
#define MY_UF_OPAQUE     0x00000008  // directory is opaque wrt. union
#define MY_UF_NOUNLINK   0x00000010  // file entry may not be removed or renamed Not implement in MacOS
#define MY_UF_COMPRESSED 0x00000020  // file entry is compressed
#define MY_UF_TRACKED    0x00000040  // notify about file entry changes
#define MY_UF_DATAVAULT  0x00000080  // entitlement required for reading and writing
#define MY_UF_HIDDEN     0x00008000  // file entry is hidden

#define MY_SF_SETTABLE   0xffff0000  // mask of superuser changeable flags
#define MY_SF_ARCHIVED   0x00010000  // file is archived
#define MY_SF_IMMUTABLE  0x00020000  // file may not be changed
#define MY_SF_APPEND     0x00040000  // writes to file may only append
#define MY_SF_RESTRICTED 0x00080000  // entitlement required for writing
#define MY_SF_NOUNLINK   0x00100000  // file entry may not be removed, renamed or used as mount point
#define MY_SF_SNAPSHOT   0x00200000  // snapshot inode
Not implement in MacOS
*/

static const char * const g_INODE_BSD_Flags[] =
{
    "UF_NODUMP"
  , "UF_IMMUTABLE"
  , "UF_APPEND"
  , "UF_OPAQUE"
  , "UF_NOUNLINK"
  , "UF_COMPRESSED"
  , "UF_TRACKED"
  , "UF_DATAVAULT"
  , NULL, NULL, NULL, NULL
  , NULL, NULL, NULL
  , "UF_HIDDEN"

  , "SF_ARCHIVE"
  , "SF_IMMUTABLE"
  , "SF_APPEND"
  , "SF_RESTRICTED"
  , "SF_NOUNLINK"
  , "SF_SNAPSHOT"
};

/*
#define INO_EXT_TYPE_SNAP_XID       1
#define INO_EXT_TYPE_DELTA_TREE_OID 2
#define INO_EXT_TYPE_DOCUMENT_ID    3
*/
#define INO_EXT_TYPE_NAME           4
/*
#define INO_EXT_TYPE_PREV_FSIZE     5
#define INO_EXT_TYPE_RESERVED_6     6
#define INO_EXT_TYPE_FINDER_INFO    7
*/
#define INO_EXT_TYPE_DSTREAM        8
/*
#define INO_EXT_TYPE_RESERVED_9     9
#define INO_EXT_TYPE_DIR_STATS_KEY  10
#define INO_EXT_TYPE_FS_UUID        11
#define INO_EXT_TYPE_RESERVED_12    12
#define INO_EXT_TYPE_SPARSE_BYTES   13
#define INO_EXT_TYPE_RDEV           14
#define INO_EXT_TYPE_PURGEABLE_FLAGS 15
#define INO_EXT_TYPE_ORIG_SYNC_ROOT_ID 16
*/


static const unsigned k_SizeOf_j_dstream = 8 * 5;

struct j_dstream
{
  UInt64 size;
  UInt64 alloced_size;
  UInt64 default_crypto_id;
  UInt64 total_bytes_written;
  UInt64 total_bytes_read;

  void Parse(const Byte *p)
  {
    G64 (0, size)
    G64 (0x8, alloced_size)
    G64 (0x10, default_crypto_id)
    G64 (0x18, total_bytes_written)
    G64 (0x20, total_bytes_read)
  }
};

static const unsigned k_SizeOf_j_file_extent_val = 8 * 3;

#define J_FILE_EXTENT_LEN_MASK    0x00ffffffffffffffU
// #define J_FILE_EXTENT_FLAG_MASK   0xff00000000000000U
// #define J_FILE_EXTENT_FLAG_SHIFT  56

#define EXTENT_GET_LEN(x) ((x) & J_FILE_EXTENT_LEN_MASK)

struct j_file_extent_val
{
  UInt64 len_and_flags;   // The length must be a multiple of the block size defined by the nx_block_size field of nx_superblock_t.
                          // There are currently no flags defined
  UInt64 phys_block_num;  // The physical block address that the extent starts at
  // UInt64 crypto_id;       // The encryption key or the encryption tweak used in this extent.
  
  void Parse(const Byte *p)
  {
    G64 (0, len_and_flags)
    G64 (0x8, phys_block_num)
    // G64 (0x10, crypto_id);
  }
};


struct CExtent
{
  UInt64 logical_offset;
  UInt64 len_and_flags;   // The length must be a multiple of the block size defined by the nx_block_size field of nx_superblock_t.
                          // There are currently no flags defined
  UInt64 phys_block_num;  // The physical block address that the extent starts at

  UInt64 GetEndOffset() const { return logical_offset + EXTENT_GET_LEN(len_and_flags); }
};


typedef UInt32  MY_uid_t;
typedef UInt32  MY_gid_t;
typedef UInt16  MY_mode_t;


typedef enum
{
  XATTR_DATA_STREAM       = 1 << 0,
  XATTR_DATA_EMBEDDED     = 1 << 1,
  XATTR_FILE_SYSTEM_OWNED = 1 << 2,
  XATTR_RESERVED_8        = 1 << 3
} j_xattr_flags;


struct CAttr
{
  AString Name;
  UInt32 flags;
  bool dstream_defined;
  bool NeedShow;
  CByteBuffer Data;

  j_dstream dstream;
  UInt64 Id;

  bool Is_dstream_OK_for_SymLink() const
  {
    return dstream_defined && dstream.size <= (1 << 12) && dstream.size != 0;
  }

  UInt64 GetSize() const
  {
    if (dstream_defined) // dstream has more priority
      return dstream.size;
    return Data.Size();
  }
  
  void Clear()
  {
    dstream_defined = false;
    NeedShow = true;
    Data.Free();
    Name.Empty();
  }

  bool Is_STREAM()   const { return (flags & XATTR_DATA_STREAM) != 0; }
  bool Is_EMBEDDED() const { return (flags & XATTR_DATA_EMBEDDED) != 0; }
};


// j_inode_val_t
struct CNode
{
  unsigned ItemIndex;      // index to CItem. We set it only if Node is directory.
  unsigned NumCalcedLinks; // Num links to that node
  // unsigned NumItems;    // Num Items in that node

  UInt64 parent_id;    // The identifier of the file system record for the parent directory.
  UInt64 private_id;
  UInt64 create_time;
  UInt64 mod_time;
  UInt64 change_time;
  UInt64 access_time;
  UInt64 internal_flags;
  union
  {
    UInt32 nchildren; /* The number of directory entries.
                         is valid only if the inode is a directory */
    UInt32 nlink;     /* The number of hard links whose target is this inode.
                         is valid only if the inode isn't a directory.
    Inodes with multiple hard links (nlink > 1)
       - The parent_id field refers to the parent directory of the primary link.
       - The name field contains the name of the primary link.
       - The INO_EXT_TYPE_NAME extended field contains the name of this link.
    */
  };
  // cp_key_class_t default_protection_class;
  UInt32 write_generation_counter;
  UInt32 bsd_flags;
  MY_uid_t owner;
  MY_gid_t group;
  MY_mode_t mode;
  UInt16 pad1;
  UInt64 uncompressed_size;

  j_dstream dstream;
  AString PrimaryName;
  
  bool dstream_defined;
  bool refcnt_defined;

  UInt32 refcnt; // j_dstream_id_val_t
  CRecordVector<CExtent> Extents;
  CObjectVector<CAttr> Attrs;
  unsigned SymLinkIndex; // index in Attrs
  unsigned DecmpfsIndex; // index in Attrs
  unsigned ResourceIndex; // index in Attrs
  
  NHfs::CCompressHeader CompressHeader;
  
  CNode():
      ItemIndex(VI_MINUS1),
      NumCalcedLinks(0),
      // NumItems(0),
      dstream_defined(false),
      refcnt_defined(false),
      SymLinkIndex(VI_MINUS1),
      DecmpfsIndex(VI_MINUS1),
      ResourceIndex(VI_MINUS1)
      {}

  bool IsDir() const { return MY_LIN_S_ISDIR(mode); }
  bool IsSymLink() const { return MY_LIN_S_ISLNK(mode); }

  bool Has_UNCOMPRESSED_SIZE() const { return (internal_flags & INODE_HAS_UNCOMPRESSED_SIZE) != 0; }

  unsigned Get_Type_From_mode() const { return mode >> 12; }

  bool GetSize(unsigned attrIndex, UInt64 &size) const
  {
    if (IsViNotDef(attrIndex))
    {
      if (dstream_defined)
      {
        size = dstream.size;
        return true;
      }
      size = 0;
      if (Has_UNCOMPRESSED_SIZE())
      {
        size = uncompressed_size;
        return true;
      }
      if (!IsSymLink())
        return false;
      attrIndex = SymLinkIndex;
      if (IsViNotDef(attrIndex))
        return false;
    }
    const CAttr &attr = Attrs[(unsigned)attrIndex];
    if (attr.dstream_defined)
      size = attr.dstream.size;
    else
      size = attr.Data.Size();
    return true;
  }
  
  bool GetPackSize(unsigned attrIndex, UInt64 &size) const
  {
    if (IsViNotDef(attrIndex))
    {
      if (dstream_defined)
      {
        size = dstream.alloced_size;
        return true;
      }
      size = 0;
      
      if (IsSymLink())
        attrIndex = SymLinkIndex;
      else
      {
        if (!CompressHeader.IsCorrect ||
            !CompressHeader.IsSupported)
          return false;
        const CAttr &attr = Attrs[DecmpfsIndex];
        if (!CompressHeader.IsMethod_Resource())
        {
          size = attr.Data.Size() - CompressHeader.DataPos;
          return true;
        }
        attrIndex = ResourceIndex;
      }
      if (IsViNotDef(attrIndex))
        return false;
    }
    const CAttr &attr = Attrs[(unsigned)attrIndex];
    if (attr.dstream_defined)
      size = attr.dstream.alloced_size;
    else
      size = attr.Data.Size();
    return true;
  }

  void Parse(const Byte *p);
};


// it's used for Attr streams
struct CSmallNode
{
  CRecordVector<CExtent> Extents;
  // UInt32 NumLinks;
  // CSmallNode(): NumLinks(0) {}
};

static const unsigned k_SizeOf_j_inode_val = 0x5c;

void CNode::Parse(const Byte *p)
{
  G64 (0, parent_id)
  G64 (0x8, private_id)
  G64 (0x10, create_time)
  G64 (0x18, mod_time)
  G64 (0x20, change_time)
  G64 (0x28, access_time)
  G64 (0x30, internal_flags)
  {
    G32 (0x38, nchildren)
    //  G32 (0x38, nlink);
  }
  // G32 (0x3c, default_protection_class);
  G32 (0x40, write_generation_counter)
  G32 (0x44, bsd_flags)
  G32 (0x48, owner)
  G32 (0x4c, group)
  G16 (0x50, mode)
  // G16 (0x52, pad1);
  G64 (0x54, uncompressed_size)
}


struct CRef
{
  unsigned ItemIndex;
  unsigned NodeIndex;
  unsigned ParentRefIndex;

 #ifdef APFS_SHOW_ALT_STREAMS
  unsigned AttrIndex;
  bool IsAltStream() const { return IsViDef(AttrIndex); }
  unsigned GetAttrIndex() const { return AttrIndex; }
 #else
  // bool IsAltStream() const { return false; }
  unsigned GetAttrIndex() const { return VI_MINUS1; }
 #endif
};


struct CRef2
{
  unsigned VolIndex;
  unsigned RefIndex;
};


struct CHashChunk
{
  UInt64 lba;
  UInt32 hashed_len; // the value is UInt16
  Byte hash[APFS_HASH_MAX_SIZE];
};

typedef CRecordVector<CHashChunk> CStreamHashes;


Z7_CLASS_IMP_NOQIB_1(
  COutStreamWithHash
  , ISequentialOutStream
)
  bool _hashError;
  CAlignedBuffer1 _sha;
  CMyComPtr<ISequentialOutStream> _stream;
  const CStreamHashes *_hashes;
  unsigned _blockSizeLog;
  unsigned _chunkIndex;
  UInt32 _offsetInChunk;
  // UInt64 _size;

  CSha256 *Sha() { return (CSha256 *)(void *)(Byte *)_sha; }
public:
  COutStreamWithHash(): _sha(sizeof(CSha256)) {}

  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  // void ReleaseStream() { _stream.Release(); }
  void Init(const CStreamHashes *hashes, unsigned blockSizeLog)
  {
    _hashes = hashes;
    _blockSizeLog = blockSizeLog;
    _chunkIndex = 0;
    _offsetInChunk = 0;
    _hashError = false;
    // _size = 0;
  }
  // UInt64 GetSize() const { return _size; }
  bool FinalCheck();
};


static bool Sha256_Final_and_CheckDigest(CSha256 *sha256, const Byte *digest)
{
  MY_ALIGN (16)
  UInt32 temp[SHA256_NUM_DIGEST_WORDS];
  Sha256_Final(sha256, (Byte *)temp);
  return memcmp(temp, digest, SHA256_DIGEST_SIZE) == 0;
}


Z7_COM7F_IMF(COutStreamWithHash::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  if (processedSize)
    *processedSize = size;
  while (size != 0)
  {
    if (_hashError)
      break;
    if (_chunkIndex >= _hashes->Size())
    {
      _hashError = true;
      break;
    }
    if (_offsetInChunk == 0)
      Sha256_Init(Sha());
    const CHashChunk &chunk = (*_hashes)[_chunkIndex];
    /* (_blockSizeLog <= 16) && chunk.hashed_len is 16-bit.
       so we can use 32-bit chunkSize here */
    const UInt32 chunkSize = ((UInt32)chunk.hashed_len << _blockSizeLog);
    const UInt32 rem = chunkSize - _offsetInChunk;
    UInt32 cur = size;
    if (cur > rem)
      cur = (UInt32)rem;
    Sha256_Update(Sha(), (const Byte *)data, cur);
    data = (const void *)((const Byte *)data + cur);
    size -= cur;
    // _size += cur;
    _offsetInChunk += cur;
    if (chunkSize == _offsetInChunk)
    {
      if (!Sha256_Final_and_CheckDigest(Sha(), chunk.hash))
        _hashError = true;
      _offsetInChunk = 0;
      _chunkIndex++;
    }
  }
  return result;
}


bool COutStreamWithHash::FinalCheck()
{
  if (_hashError)
    return false;

  if (_offsetInChunk != 0)
  {
    const CHashChunk &chunk = (*_hashes)[_chunkIndex];
    {
      const UInt32 chunkSize = ((UInt32)chunk.hashed_len << _blockSizeLog);
      const UInt32 rem = chunkSize - _offsetInChunk;
      Byte b = 0;
      for (UInt32 i = 0; i < rem; i++)
        Sha256_Update(Sha(), &b, 1);
    }
    if (!Sha256_Final_and_CheckDigest(Sha(), chunk.hash))
      _hashError = true;
    _offsetInChunk = 0;
    _chunkIndex++;
  }
  if (_chunkIndex != _hashes->Size())
    _hashError = true;
  return !_hashError;
}



struct CVol
{
  CObjectVector<CNode> Nodes;
  CRecordVector<UInt64> NodeIDs;
  CObjectVector<CItem> Items;
  CRecordVector<CRef> Refs;

  CObjectVector<CSmallNode> SmallNodes;
  CRecordVector<UInt64> SmallNodeIDs;

  CObjectVector<CSmallNode> FEXT_Nodes;
  CRecordVector<UInt64> FEXT_NodeIDs;

  CObjectVector<CStreamHashes> Hash_Vectors;
  CRecordVector<UInt64> Hash_IDs;

  unsigned StartRef2Index;  // ref2_Index for Refs[0] item
  unsigned RootRef2Index;   // ref2_Index of virtual root folder (Volume1)
  CApfs apfs;
  C_integrity_meta_phys integrity;

  bool NodeNotFound;
  bool ThereAreUnlinkedNodes;
  bool WrongInodeLink;
  bool UnsupportedFeature;
  bool UnsupportedMethod;

  unsigned NumItems_In_PrivateDir;
  unsigned NumAltStreams;

  UString RootName;

  bool ThereAreErrors() const
  {
    return NodeNotFound || ThereAreUnlinkedNodes || WrongInodeLink;
  }

  void AddComment(UString &s) const;

  HRESULT FillRefs();
  
  CVol():
      StartRef2Index(0),
      RootRef2Index(VI_MINUS1),
      NodeNotFound(false),
      ThereAreUnlinkedNodes(false),
      WrongInodeLink(false),
      UnsupportedFeature(false),
      UnsupportedMethod(false),
      NumItems_In_PrivateDir(0),
      NumAltStreams(0)
      {}
};


static void ApfsTimeToFileTime(UInt64 apfsTime, FILETIME &ft, UInt32 &ns100)
{
  const UInt64 s = apfsTime / 1000000000;
  const UInt32 ns = (UInt32)(apfsTime % 1000000000);
  ns100 = (ns % 100);
  const UInt64 v = NWindows::NTime::UnixTime64_To_FileTime64((Int64)s) + ns / 100;
  ft.dwLowDateTime = (DWORD)v;
  ft.dwHighDateTime = (DWORD)(v >> 32);
}

static void AddComment_Name(UString &s, const char *name)
{
  s += name;
  s += ": ";
}

/*
static void AddComment_Bool(UString &s, const char *name, bool val)
{
  AddComment_Name(s, name);
  s += val ? "+" : "-";
  s.Add_LF();
}
*/

static void AddComment_UInt64(UString &s, const char *name, UInt64 v)
{
  AddComment_Name(s, name);
  s.Add_UInt64(v);
  s.Add_LF();
}


static void AddComment_Time(UString &s, const char *name, UInt64 v)
{
  AddComment_Name(s, name);

  FILETIME ft;
  UInt32 ns100;
  ApfsTimeToFileTime(v, ft, ns100);
  char temp[64];
  ConvertUtcFileTimeToString2(ft, ns100, temp
    // , kTimestampPrintLevel_SEC);
    , kTimestampPrintLevel_NS);
  s += temp;
  s.Add_LF();
}


static void AddComment_modified_by_t(UString &s, const char *name, const apfs_modified_by_t &v)
{
  AddComment_Name(s, name);
  AString s2;
  s2.SetFrom_CalcLen((const char *)v.id, sizeof(v.id));
  s += s2;
  s.Add_LF();
  s += "  ";
  AddComment_Time(s, "timestamp", v.timestamp);
  s += "  ";
  AddComment_UInt64(s, "last_xid", v.last_xid);
}


static void AddVolInternalName_toString(UString &s, const CApfs &apfs)
{
  AString temp;
  temp.SetFrom_CalcLen((const char *)apfs.volname, sizeof(apfs.volname));
  UString unicode;
  ConvertUTF8ToUnicode(temp, unicode);
  s += unicode;
}


void CVol::AddComment(UString &s) const
{
  AddComment_UInt64(s, "fs_index", apfs.fs_index);
  {
    AddComment_Name(s, "volume_name");
    AddVolInternalName_toString(s, apfs);
    s.Add_LF();
  }
  AddComment_Name(s, "vol_uuid");
  apfs.vol_uuid.AddHexToString(s);
  s.Add_LF();

  AddComment_Name(s, "incompatible_features");
  s += FlagsToString(g_APFS_INCOMPAT_Flags,
      Z7_ARRAY_SIZE(g_APFS_INCOMPAT_Flags),
      (UInt32)apfs.incompatible_features);
  s.Add_LF();


  if (apfs.integrity_meta_oid != 0)
  {
    /*
    AddComment_Name(s, "im_version");
    s.Add_UInt32(integrity.im_version);
    s.Add_LF();
    */
    AddComment_Name(s, "im_flags");
    s.Add_UInt32(integrity.im_flags);
    s.Add_LF();
    AddComment_Name(s, "im_hash_type");
    const char *p = NULL;
    if (integrity.im_hash_type < Z7_ARRAY_SIZE(g_hash_types))
      p = g_hash_types[integrity.im_hash_type];
    if (p)
      s += p;
    else
      s.Add_UInt32(integrity.im_hash_type);
    s.Add_LF();
  }


  // AddComment_UInt64(s, "reserve_block_count", apfs.fs_reserve_block_count, false);
  // AddComment_UInt64(s, "quota_block_count", apfs.fs_quota_block_count);
  AddComment_UInt64(s, "fs_alloc_count", apfs.fs_alloc_count);

  AddComment_UInt64(s, "num_files", apfs.num_files);
  AddComment_UInt64(s, "num_directories", apfs.num_directories);
  AddComment_UInt64(s, "num_symlinks", apfs.num_symlinks);
  AddComment_UInt64(s, "num_other_fsobjects", apfs.num_other_fsobjects);

  AddComment_UInt64(s, "Num_Attr_Streams", NumAltStreams);

  AddComment_UInt64(s, "num_snapshots", apfs.num_snapshots);
  AddComment_UInt64(s, "total_blocks_alloced", apfs.total_blocks_alloced);
  AddComment_UInt64(s, "total_blocks_freed", apfs.total_blocks_freed);

  AddComment_Time(s, "unmounted", apfs.unmount_time);
  AddComment_Time(s, "last_modified", apfs.last_mod_time);
  AddComment_modified_by_t(s, "formatted_by", apfs.formatted_by);
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(apfs.modified_by); i++)
  {
    const apfs_modified_by_t &v = apfs.modified_by[i];
    if (v.last_xid == 0 && v.timestamp == 0 && v.id[0] == 0)
      continue;
    AString name ("modified_by[");
    name.Add_UInt32(i);
    name += ']';
    AddComment_modified_by_t(s, name.Ptr(), v);
  }
}



struct CKeyValPair
{
  CByteBuffer Key;
  CByteBuffer Val;
  // unsigned ValPos; // for alognment
};


struct omap_key
{
  oid_t oid; // The object identifier
  xid_t xid; // The transaction identifier
  void Parse(const Byte *p)
  {
    G64o (0, oid)
    G64x (8, xid)
  }
};

/*
#define OMAP_VAL_DELETED            (1 << 0)
#define OMAP_VAL_SAVED              (1 << 1)
#define OMAP_VAL_ENCRYPTED          (1 << 2)
*/
#define OMAP_VAL_NOHEADER           (1 << 3)
/*
#define OMAP_VAL_CRYPTO_GENERATION  (1 << 4)
*/

struct omap_val
{
  UInt32 flags;
  UInt32 size;
  // paddr_t paddr;
  paddr_t_unsigned paddr;

  bool IsFlag_NoHeader() const { return (flags & OMAP_VAL_NOHEADER) != 0; }
  
  void Parse(const Byte *p)
  {
    G32 (0, flags)
    G32 (4, size)
    G64 (8, paddr)
  }
};


struct CObjectMap
{
  CRecordVector<UInt64> Keys;
  CRecordVector<omap_val> Vals;

  bool Parse(const CObjectVector<CKeyValPair> &pairs);
  int FindKey(UInt64 id) const { return Keys.FindInSorted(id); }
};

bool CObjectMap::Parse(const CObjectVector<CKeyValPair> &pairs)
{
  omap_key prev;
  prev.oid = 0;
  prev.xid = 0;
  FOR_VECTOR (i, pairs)
  {
    const CKeyValPair &pair = pairs[i];
    if (pair.Key.Size() != 16 || pair.Val.Size() != 16)
      return false;
    omap_key key;
    key.Parse(pair.Key);
    omap_val val;
    val.Parse(pair.Val);
    /* Object map B-trees are sorted by object identifier and then by transaction identifier
       but it's possible to have identical Ids in map ?
       do we need to look transaction id ?
       and search key with largest transaction id? */
    if (key.oid <= prev.oid)
      return false;
    prev = key;
    Keys.Add(key.oid);
    Vals.Add(val);
  }
  return true;
}


struct CMap
{
  CObjectVector<CKeyValPair> Pairs;
  
  CObjectMap Omap;
  btree_info bti;
  UInt64 NumNodes;

  // we use thnese options to check:
  UInt32 Subtype;
  bool IsPhysical;
  
  bool CheckAtFinish() const
  {
    return NumNodes == bti.node_count && Pairs.Size() == bti.key_count;
  }

  CMap():
      NumNodes(0),
      Subtype(0),
      IsPhysical(true)
      {}
};



struct CDatabase
{
  CRecordVector<CRef2> Refs2;
  CObjectVector<CVol> Vols;

  bool HeadersError;
  bool ThereAreAltStreams;
  bool UnsupportedFeature;
  bool UnsupportedMethod;
  
  CSuperBlock sb;

  IInStream *OpenInStream;
  IArchiveOpenCallback *OpenCallback;
  UInt64 ProgressVal_Cur;
  UInt64 ProgressVal_Prev;
  UInt64 ProgressVal_NumFilesTotal;
  CObjectVector<CByteBuffer> Buffers;

  UInt32 MethodsMask;
  UInt64 GetSize(const UInt32 index) const;

  void Clear()
  {
    HeadersError = false;
    ThereAreAltStreams = false;
    UnsupportedFeature = false;
    UnsupportedMethod = false;
    
    ProgressVal_Cur = 0;
    ProgressVal_Prev = 0;
    ProgressVal_NumFilesTotal = 0;

    MethodsMask = 0;
    
    Vols.Clear();
    Refs2.Clear();
    Buffers.Clear();
  }

  HRESULT SeekReadBlock_FALSE(UInt64 oid, void *data);
  void GetItemPath(unsigned index, const CNode *inode, NWindows::NCOM::CPropVariant &path) const;
  HRESULT ReadMap(UInt64 oid, bool noHeader, CVol *vol, const Byte *hash,
      CMap &map, unsigned recurseLevel);
  HRESULT ReadObjectMap(UInt64 oid, CVol *vol, CObjectMap &map);
  HRESULT OpenVolume(const CObjectMap &omap, const oid_t fs_oid);
  HRESULT Open2();

  HRESULT GetAttrStream(IInStream *apfsInStream, const CVol &vol,
      const CAttr &attr, ISequentialInStream **stream);

  HRESULT GetAttrStream_dstream(IInStream *apfsInStream, const CVol &vol,
      const CAttr &attr, ISequentialInStream **stream);

  HRESULT GetStream2(
      IInStream *apfsInStream,
      const CRecordVector<CExtent> *extents, UInt64 rem,
      ISequentialInStream **stream);
};


HRESULT CDatabase::SeekReadBlock_FALSE(UInt64 oid, void *data)
{
  if (OpenCallback)
  {
    if (ProgressVal_Cur - ProgressVal_Prev >= (1 << 22))
    {
      RINOK(OpenCallback->SetCompleted(NULL, &ProgressVal_Cur))
      ProgressVal_Prev = ProgressVal_Cur;
    }
    ProgressVal_Cur += sb.block_size;
  }
  if (oid == 0 || oid >= sb.block_count)
    return S_FALSE;
  RINOK(InStream_SeekSet(OpenInStream, oid << sb.block_size_Log))
  return ReadStream_FALSE(OpenInStream, data, sb.block_size);
}



API_FUNC_static_IsArc IsArc_APFS(const Byte *p, size_t size)
{
  if (size < kApfsHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  CSuperBlock sb;
  if (!sb.Parse(p))
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}


static bool CheckHash(unsigned hashAlgo, const Byte *data, size_t size, const Byte *digest)
{
  if (hashAlgo == APFS_HASH_SHA256)
  {
    MY_ALIGN (16)
    CSha256 sha;
    Sha256_Init(&sha);
    Sha256_Update(&sha, data, size);
    return Sha256_Final_and_CheckDigest(&sha, digest);
  }
  return true;
}

 
HRESULT CDatabase::ReadMap(UInt64 oid, bool noHeader,
    CVol *vol, const Byte *hash,
    CMap &map, unsigned recurseLevel)
{
  // is it allowed to use big number of levels ?
  if (recurseLevel > (1 << 10))
    return S_FALSE;

  const UInt32 blockSize = sb.block_size;
  if (Buffers.Size() <= recurseLevel)
  {
    Buffers.AddNew();
    if (Buffers.Size() <= recurseLevel)
      throw 123;
    Buffers.Back().Alloc(blockSize);
  }
  const Byte *buf;
  {
    CByteBuffer &buf2 = Buffers[recurseLevel];
    RINOK(SeekReadBlock_FALSE(oid, buf2))
    buf = buf2;
  }

  if (hash && vol)
    if (!CheckHash(vol->integrity.im_hash_type, buf, blockSize, hash))
      return S_FALSE;

  CBTreeNodePhys bt;
  if (!bt.Parse(buf, blockSize, noHeader))
    return S_FALSE;
  
  map.NumNodes++;

  /* Specification: All values are stored in leaf nodes, which
     makes these B+ trees, and the values in nonleaf nodes are object
     identifiers of child nodes.
  
     The entries in the table of contents are sorted by key. The comparison function used for sorting depends on the keys type
      - Object map B-trees are sorted by object identifier and then by transaction identifier.
      - Free queue B-trees are sorted by transaction identifier and then by physical address.
      - File-system records are sorted according to the rules listed in File-System Objects.
  */
  
  if (!noHeader)
  if (bt.ophys.subtype != map.Subtype)
    return S_FALSE;

  unsigned endLimit = blockSize;

  if (recurseLevel == 0)
  {
    if (!noHeader)
    if (bt.ophys.GetType() != OBJECT_TYPE_BTREE)
      return S_FALSE;
    if ((bt.flags & BTNODE_ROOT) == 0)
      return S_FALSE;
    endLimit -= k_btree_info_Size;
    map.bti.Parse(buf + endLimit);
    btree_info &bti = map.bti;
    if (bti.fixed.key_size >= blockSize)
      return S_FALSE;
    if (bti.Is_EPHEMERAL() &&
        bti.Is_PHYSICAL())
      return S_FALSE;
    if (bti.Is_PHYSICAL() != map.IsPhysical)
      return S_FALSE;
    if (bti.Is_NOHEADER() != noHeader)
      return S_FALSE;
    // we don't allow volumes with big number of Keys
    const UInt32 kNumItemsMax = k_VectorSizeMax;
    if (map.bti.node_count > kNumItemsMax)
      return S_FALSE;
    if (map.bti.key_count > kNumItemsMax)
      return S_FALSE;
  }
  else
  {
    if (!noHeader)
    if (bt.ophys.GetType() != OBJECT_TYPE_BTREE_NODE)
      return S_FALSE;
    if ((bt.flags & BTNODE_ROOT) != 0)
      return S_FALSE;
    if (map.NumNodes > map.bti.node_count
        || map.Pairs.Size() > map.bti.key_count)
      return S_FALSE;
  }

  const bool isLeaf = (bt.flags & BTNODE_LEAF) != 0;

  if (isLeaf)
  {
    if (bt.level != 0)
      return S_FALSE;
  }
  else
  {
    if (bt.level == 0)
      return S_FALSE;
  }

  if (!bt.table_space.CheckOverLimit(endLimit - k_Toc_offset))
    return S_FALSE;

  const unsigned tableEnd = k_Toc_offset + bt.table_space.GetEnd();
  const unsigned keyValRange = endLimit - tableEnd;
  const unsigned tocEntrySize = bt.Is_FIXED_KV_SIZE() ? 4 : 8;
  if (bt.table_space.len / tocEntrySize < bt.nkeys)
    return S_FALSE;

  for (unsigned i = 0; i < bt.nkeys; i++)
  {
    const Byte *p = buf + k_Toc_offset + bt.table_space.off + i * tocEntrySize;
    if (bt.Is_FIXED_KV_SIZE())
    {
      kvoff a;
      a.Parse(p);
      if (a.k + map.bti.fixed.key_size > keyValRange
          || a.v > keyValRange)
        return S_FALSE;
      {
        CKeyValPair pair;
        
        const Byte *p2 = buf + k_Toc_offset + bt.table_space.len;
        p2 += a.k;
        pair.Key.CopyFrom(p2, map.bti.fixed.key_size);

        p2 = buf + endLimit;
        p2 -= a.v;
        if (isLeaf)
        {
          if (a.v < map.bti.fixed.val_size)
            return S_FALSE;
          pair.Val.CopyFrom(p2, map.bti.fixed.val_size);
          // pair.ValPos = endLimit - a.v;
          map.Pairs.Add(pair);
          continue;
        }
        {
          if (a.v < 8)
            return S_FALSE;
          // value is only 64-bit for non leaf.
          const oid_t oidNext = Get64(p2);
          if (map.bti.Is_PHYSICAL())
          {
            RINOK(ReadMap(oidNext, noHeader, vol,
                NULL, /* hash */
                map, recurseLevel + 1))
            continue;
          }
          else
          {
            // fixme
            return S_FALSE;
          }
        }
      }
    }
    else
    {
      kvloc a;
      a.Parse(p);
      if (!a.k.CheckOverLimit(keyValRange)
          || a.v.off > keyValRange
          || a.v.len > a.v.off)
        return S_FALSE;
      {
        CKeyValPair pair;
        const Byte *p2 = buf + k_Toc_offset + bt.table_space.len;
        p2 += a.k.off;
        pair.Key.CopyFrom(p2, a.k.len);

        p2 = buf + endLimit;
        p2 -= a.v.off;
        if (isLeaf)
        {
          pair.Val.CopyFrom(p2, a.v.len);
          // pair.ValPos = endLimit - a.v.off;
          map.Pairs.Add(pair);
          continue;
        }
        {
          if (a.v.len < 8)
            return S_FALSE;

          const Byte *hashNew = NULL;
          oid_t oidNext = Get64(p2);

          if (bt.Is_HASHED())
          {
            if (!vol)
              return S_FALSE;
            /*
            struct btn_index_node_val {
              oid_t binv_child_oid;
              uint8_t binv_child_hash[BTREE_NODE_HASH_SIZE_MAX];
            };
            */
            /* (a.v.len == 40) is possible if Is_HASHED()
               so there is hash (for example, 256-bit) after 64-bit id) */
            if (a.v.len != 8 + vol->integrity.HashSize)
              return S_FALSE;
            hashNew = p2 + 8;
            /* we need to add root_tree_oid here,
               but where it's defined in apfs specification ? */
            oidNext += vol->apfs.root_tree_oid;
          }
          else
          {
            if (a.v.len != 8)
              return S_FALSE;
          }

          // value is only 64-bit for non leaf.

          if (map.bti.Is_PHYSICAL())
          {
            return S_FALSE;
            // the code was not tested:
            // RINOK(ReadMap(oidNext, map, recurseLevel + 1));
            // continue;
          }
          /*
          else if (map.bti.Is_EPHEMERAL())
          {
            // Ephemeral objects are stored in memory while the container is mounted and in a checkpoint when the container isn't mounted
            // the code was not tested:
            return S_FALSE;
          }
          */
          else
          {
            /* nodes in the B-tree use virtual object identifiers to link to their child nodes. */
            const int index = map.Omap.FindKey(oidNext);
            if (index == -1)
              return S_FALSE;
            const omap_val &ov = map.Omap.Vals[(unsigned)index];
            if (ov.size != blockSize) // change it : it must be multiple of
              return S_FALSE;
            RINOK(ReadMap(ov.paddr, ov.IsFlag_NoHeader(), vol, hashNew,
                map, recurseLevel + 1))
            continue;
          }
        }
      }
    }
  }

  if (recurseLevel == 0)
    if (!map.CheckAtFinish())
      return S_FALSE;
  return S_OK;
}



HRESULT CDatabase::ReadObjectMap(UInt64 oid, CVol *vol, CObjectMap &omap)
{
  CByteBuffer buf;
  const size_t blockSize = sb.block_size;
  buf.Alloc(blockSize);
  RINOK(SeekReadBlock_FALSE(oid, buf))
  C_omap_phys op;
  if (!op.Parse(buf, blockSize, oid))
    return S_FALSE;
  CMap map;
  map.Subtype = OBJECT_TYPE_OMAP;
  map.IsPhysical = true;
  RINOK(ReadMap(op.tree_oid,
      false /* noHeader */,
      vol,
      NULL, /* hash */
      map, 0))
  if (!omap.Parse(map.Pairs))
    return S_FALSE;
  return S_OK;
}



HRESULT CDatabase::Open2()
{
  Clear();
  CSuperBlock2 sb2;
  {
    Byte buf[kApfsHeaderSize];
    RINOK(ReadStream_FALSE(OpenInStream, buf, kApfsHeaderSize))
    if (!sb.Parse(buf))
      return S_FALSE;
    sb2.Parse(buf);
  }

  {
    CObjectMap omap;
    RINOK(ReadObjectMap(sb.omap_oid,
        NULL, /* CVol */
        omap))
    unsigned numRefs = 0;
    for (unsigned i = 0; i < sb.max_file_systems; i++)
    {
      const oid_t oid = sb2.fs_oid[i];
      if (oid == 0)
        continue;
      // for (unsigned k = 0; k < 1; k++) // for debug
      RINOK(OpenVolume(omap, oid))
      const unsigned a = Vols.Back().Refs.Size();
      numRefs += a;
      if (numRefs < a)
        return S_FALSE; // overflow
    }
  }

  const bool needVolumePrefix = (Vols.Size() > 1);
  // const bool needVolumePrefix = true; // for debug
  {
    unsigned numRefs = 0;
    FOR_VECTOR (i, Vols)
    {
      const unsigned a = Vols[i].Refs.Size();
      numRefs += a;
      if (numRefs < a)
        return S_FALSE; // overflow
    }
    numRefs += Vols.Size();
    if (numRefs < Vols.Size())
      return S_FALSE; // overflow
    Refs2.Reserve(numRefs);
  }
  {
    FOR_VECTOR (i, Vols)
    {
      CVol &vol = Vols[i];

      CRef2 ref2;
      ref2.VolIndex = i;
      
      if (needVolumePrefix)
      {
        vol.RootName = "Volume";
        vol.RootName.Add_UInt32(1 + (UInt32)i);
        vol.RootRef2Index = Refs2.Size();
        ref2.RefIndex = VI_MINUS1;
        Refs2.Add(ref2);
      }

      vol.StartRef2Index = Refs2.Size();
      const unsigned numItems = vol.Refs.Size();
      for (unsigned k = 0; k < numItems; k++)
      {
        ref2.RefIndex = k;
        Refs2.Add(ref2);
      }
    }
  }
  return S_OK;
}


HRESULT CDatabase::OpenVolume(const CObjectMap &omap, const oid_t fs_oid)
{
  const size_t blockSize = sb.block_size;
  CByteBuffer buf;
  {
    const int index = omap.FindKey(fs_oid);
    if (index == -1)
      return S_FALSE;
    const omap_val &ov = omap.Vals[(unsigned)index];
    if (ov.size != blockSize) // change it : it must be multiple of
      return S_FALSE;
    buf.Alloc(blockSize);
    RINOK(SeekReadBlock_FALSE(ov.paddr, buf))
  }

  CVol &vol = Vols.AddNew();
  CApfs &apfs = vol.apfs;

  if (!apfs.Parse(buf, blockSize))
    return S_FALSE;

  if (apfs.fext_tree_oid != 0)
  {
    if ((apfs.incompatible_features & APFS_INCOMPAT_SEALED_VOLUME) == 0)
      return S_FALSE;
    if ((apfs.fext_tree_type & OBJ_PHYSICAL) == 0)
      return S_FALSE;
    if ((apfs.fext_tree_type & OBJECT_TYPE_MASK) != OBJECT_TYPE_BTREE)
      return S_FALSE;

    CMap map2;
    map2.Subtype = OBJECT_TYPE_FEXT_TREE;
    map2.IsPhysical = true;

    RINOK(ReadMap(apfs.fext_tree_oid,
        false /* noHeader */,
        &vol,
        NULL, /* hash */
        map2, 0))

    UInt64 prevId = 1;

    FOR_VECTOR (i, map2.Pairs)
    {
      if (OpenCallback && (i & 0xffff) == 1)
      {
        const UInt64 numFiles = ProgressVal_NumFilesTotal +
            (vol.Items.Size() + vol.Nodes.Size()) / 2;
        RINOK(OpenCallback->SetCompleted(&numFiles, &ProgressVal_Cur))
      }
      // The key half of a record from a file extent tree.
      // struct fext_tree_key
      const CKeyValPair &pair = map2.Pairs[i];
      if (pair.Key.Size() != 16)
        return S_FALSE;
      const Byte *p = pair.Key;
      const UInt64 id = GetUi64(p);
      if (id < prevId)
        return S_FALSE;  // IDs must be sorted
      prevId = id;

      CExtent ext;
      ext.logical_offset = GetUi64(p + 8);
      {
        if (pair.Val.Size() != 16)
          return S_FALSE;
        const Byte *p2 = pair.Val;
        ext.len_and_flags = GetUi64(p2);
        ext.phys_block_num = GetUi64(p2 + 8);
      }

      PRF(printf("\n%6d: id=%6d logical_addr = %2d len_and_flags=%5x phys_block_num = %5d",
          i, (unsigned)id,
          (unsigned)ext.logical_offset,
          (unsigned)ext.len_and_flags,
          (unsigned)ext.phys_block_num));

      if (vol.FEXT_NodeIDs.IsEmpty() ||
          vol.FEXT_NodeIDs.Back() != id)
      {
        vol.FEXT_NodeIDs.Add(id);
        vol.FEXT_Nodes.AddNew();
      }
      CRecordVector<CExtent> &extents = vol.FEXT_Nodes.Back().Extents;
      if (!extents.IsEmpty())
        if (ext.logical_offset != extents.Back().GetEndOffset())
          return S_FALSE;
      extents.Add(ext);
      continue;
    }

    PRF(printf("\n\n"));
  }

  /* For each volume, read the root file system tree's virtual object
     identifier from the apfs_root_tree_oid field,
     and then look it up in the volume object map indicated
     by the omap_oid field. */

  CMap map;
  ReadObjectMap(apfs.omap_oid, &vol, map.Omap);

  const Byte *hash_for_root = NULL;

  if (apfs.integrity_meta_oid != 0)
  {
    if ((apfs.incompatible_features & APFS_INCOMPAT_SEALED_VOLUME) == 0)
      return S_FALSE;
    const int index = map.Omap.FindKey(apfs.integrity_meta_oid);
    if (index == -1)
      return S_FALSE;
    const omap_val &ov = map.Omap.Vals[(unsigned)index];
    if (ov.size != blockSize)
      return S_FALSE;
    RINOK(SeekReadBlock_FALSE(ov.paddr, buf))
    if (!vol.integrity.Parse(buf, blockSize, apfs.integrity_meta_oid))
      return S_FALSE;
    if (vol.integrity.im_hash_type != 0)
      hash_for_root = vol.integrity.Hash;
  }

  {
    const int index = map.Omap.FindKey(apfs.root_tree_oid);
    if (index == -1)
      return S_FALSE;
    const omap_val &ov = map.Omap.Vals[(unsigned)index];
    if (ov.size != blockSize) // change it : it must be multiple of
      return S_FALSE;
    map.Subtype = OBJECT_TYPE_FSTREE;
    map.IsPhysical = false;
    RINOK(ReadMap(ov.paddr, ov.IsFlag_NoHeader(),
        &vol, hash_for_root,
        map, 0))
  }

  bool needParseAttr = false;

  {
    const bool isHashed = apfs.IsHashedName();
    UInt64 prevId = 1;

    {
      const UInt64 numApfsItems = vol.apfs.GetTotalItems()
          + 2;   // we will have 2 additional hidden directories: root and private-dir
      const UInt64 numApfsItems_Reserve = numApfsItems
          + 16;  // we reserve 16 for some possible unexpected items
      if (numApfsItems_Reserve < map.Pairs.Size())
      {
        vol.Items.ClearAndReserve((unsigned)numApfsItems_Reserve);
        vol.Nodes.ClearAndReserve((unsigned)numApfsItems_Reserve);
        vol.NodeIDs.ClearAndReserve((unsigned)numApfsItems_Reserve);
      }
      if (OpenCallback)
      {
        const UInt64 numFiles = ProgressVal_NumFilesTotal + numApfsItems;
        RINOK(OpenCallback->SetTotal(&numFiles, NULL))
      }
    }

    CAttr attr;
    CItem item;

    FOR_VECTOR (i, map.Pairs)
    {
      if (OpenCallback && (i & 0xffff) == 1)
      {
        const UInt64 numFiles = ProgressVal_NumFilesTotal +
            (vol.Items.Size() + vol.Nodes.Size()) / 2;
        RINOK(OpenCallback->SetCompleted(&numFiles, &ProgressVal_Cur))
      }

      const CKeyValPair &pair = map.Pairs[i];
      j_key_t jkey;
      if (pair.Key.Size() < 8)
        return S_FALSE;
      const Byte *p = pair.Key;
      jkey.Parse(p);
      const unsigned type = jkey.GetType();
      const UInt64 id = jkey.GetID();
      if (id < prevId)
        return S_FALSE;  // IDs must be sorted
      prevId = id;

      PRF(printf("\n%6d: id=%6d type = %2d ", i, (unsigned)id, type));
      
      if (type == APFS_TYPE_INODE)
      {
        PRF(printf("INODE"));
        if (pair.Key.Size() != 8 ||
            pair.Val.Size() < k_SizeOf_j_inode_val)
          return S_FALSE;

        CNode inode;
        inode.Parse(pair.Val);

        if (inode.private_id != id)
        {
          /* private_id : The unique identifier used by this file's data stream.
             This identifier appears in the owning_obj_id field of j_phys_ext_val_t
             records that describe the extents where the data is stored.
             For an inode that doesn't have data, the value of this
             field is the file-system object's identifier.
          */
          // APFS_TYPE_EXTENT allow to link physical address extents.
          // we don't support case (private_id != id)
          UnsupportedFeature = true;
          // return S_FALSE;
        }
        const UInt32 extraSize = (UInt32)pair.Val.Size() - k_SizeOf_j_inode_val;
        if (extraSize != 0)
        {
          if (extraSize < 4)
            return S_FALSE;
          /*
          struct xf_blob
          {
            uint16_t xf_num_exts;
            uint16_t xf_used_data;
            uint8_t xf_data[];
          };
          */
          const Byte *p2 = pair.Val + k_SizeOf_j_inode_val;
          const UInt32 xf_num_exts = Get16(p2);
          const UInt32 xf_used_data = Get16(p2 + 2);
          UInt32 offset = 4 + (UInt32)xf_num_exts * 4;
          if (offset + xf_used_data != extraSize)
            return S_FALSE;

          PRF(printf(" parent_id = %d", (unsigned)inode.parent_id));

          for (unsigned k = 0; k < xf_num_exts; k++)
          {
            // struct x_field
            const Byte *p3 = p2 + 4 + k * 4;
            const Byte x_type = p3[0];
            // const Byte x_flags = p3[1];
            const UInt32 x_size = Get16(p3 + 2);
            const UInt32 x_size_ceil = (x_size + 7) & ~(UInt32)7;
            if (offset + x_size_ceil > extraSize)
              return S_FALSE;
            const Byte *p4 = p2 + offset;
            if (x_type == INO_EXT_TYPE_NAME)
            {
              if (x_size < 2)
                return S_FALSE;
              inode.PrimaryName.SetFrom_CalcLen((const char *)p4, x_size);
              PRF(printf(" PrimaryName = %s", inode.PrimaryName.Ptr()));
              if (inode.PrimaryName.Len() != x_size - 1)
                HeadersError = true;
              // return S_FALSE;
            }
            else if (x_type == INO_EXT_TYPE_DSTREAM)
            {
              if (x_size != k_SizeOf_j_dstream)
                return S_FALSE;
              if (inode.dstream_defined)
                return S_FALSE;
              inode.dstream.Parse(p4);
              inode.dstream_defined = true;
            }
            else
            {
              // UnsupportedFeature = true;
              // return S_FALSE;
            }
            offset += x_size_ceil;
          }
          if (offset != extraSize)
            return S_FALSE;
        }

        if (!vol.NodeIDs.IsEmpty())
          if (id <= vol.NodeIDs.Back())
            return S_FALSE;
        vol.Nodes.Add(inode);
        vol.NodeIDs.Add(id);
        continue;
      }

      if (type == APFS_TYPE_XATTR)
      {
        PRF(printf("  XATTR"));

        /*
        struct j_xattr_key
        {
          j_key_t hdr;
          uint16_t name_len;
          uint8_t name[0];
        }
        */

        UInt32 len;
        unsigned nameOffset;
        {
          nameOffset = 8 + 2;
          if (pair.Key.Size() < nameOffset + 1)
            return S_FALSE;
          len = Get16(p + 8);
        }
        if (nameOffset + len != pair.Key.Size())
          return S_FALSE;

        attr.Clear();
        attr.Name.SetFrom_CalcLen((const char *)p + nameOffset, len);
        if (attr.Name.Len() != len - 1)
          return S_FALSE;

        PRF(printf("  name=%s", attr.Name.Ptr()));

        const unsigned k_SizeOf_j_xattr_val = 4;
        if (pair.Val.Size() < k_SizeOf_j_xattr_val)
          return S_FALSE;
        /*
        struct j_xattr_val
        {
          uint16_t flags;
          uint16_t xdata_len;
          uint8_t xdata[0];
        }
        */
        attr.flags = Get16(pair.Val);
        const UInt32 xdata_len = Get16(pair.Val + 2);

        PRF(printf("  flags=%x xdata_len = %d",
            (unsigned)attr.flags,
            (unsigned)xdata_len));

        const Byte *p4 = pair.Val + 4;

        if (k_SizeOf_j_xattr_val + xdata_len != pair.Val.Size())
          return S_FALSE;
        if (attr.Is_EMBEDDED())
          attr.Data.CopyFrom(p4, xdata_len);
        else if (attr.Is_STREAM())
        {
          // why (attr.flags == 0x11) here? (0x11 is undocummented flag)
          if (k_SizeOf_j_xattr_val + 8 + k_SizeOf_j_dstream != pair.Val.Size())
            return S_FALSE;
          attr.Id = Get64(p4);
          attr.dstream.Parse(p4 + 8);
          attr.dstream_defined = true;
          PRF(printf(" streamID=%d streamSize=%d", (unsigned)attr.Id, (unsigned)attr.dstream.size));
        }
        else
        {
          // unknown attribute
          // UnsupportedFeature = true;
          // return S_FALSE;
        }

        if (vol.NodeIDs.IsEmpty() ||
            vol.NodeIDs.Back() != id)
        {
          return S_FALSE;
          // UnsupportedFeature = true;
          // continue;
        }
        
        CNode &inode = vol.Nodes.Back();
        
        if (attr.Name.IsEqualTo("com.apple.fs.symlink"))
        {
          inode.SymLinkIndex = inode.Attrs.Size();
          if (attr.Is_dstream_OK_for_SymLink())
            needParseAttr = true;
        }
        else if (attr.Name.IsEqualTo("com.apple.decmpfs"))
        {
          inode.DecmpfsIndex = inode.Attrs.Size();
          // if (attr.dstream_defined)
          needParseAttr = true;
        }
        else if (attr.Name.IsEqualTo("com.apple.ResourceFork"))
        {
          inode.ResourceIndex = inode.Attrs.Size();
        }
        inode.Attrs.Add(attr);
        continue;
      }

      if (type == APFS_TYPE_DSTREAM_ID)
      {
        PRF(printf("  DSTREAM_ID"));
        if (pair.Key.Size() != 8)
          return S_FALSE;
        // j_dstream_id_val_t
        if (pair.Val.Size() != 4)
          return S_FALSE;
        const UInt32 refcnt = Get32(pair.Val);

        // The data stream record can be deleted when its reference count reaches zero.
        PRF(printf("  refcnt = %d", (unsigned)refcnt));

        if (vol.NodeIDs.IsEmpty())
          return S_FALSE;
        
        if (vol.NodeIDs.Back() != id)
        {
          // is it possible ?
          // continue;
          return S_FALSE;
        }
        
        CNode &inode = vol.Nodes.Back();

        if (inode.refcnt_defined)
          return S_FALSE;

        inode.refcnt = refcnt;
        inode.refcnt_defined = true;
        if (inode.refcnt != (UInt32)inode.nlink)
        {
          PRF(printf(" refcnt != nlink"));
          // is it possible ?
          // return S_FALSE;
        }
        continue;
      }
      
      if (type == APFS_TYPE_FILE_EXTENT)
      {
        PRF(printf("  FILE_EXTENT"));
        /*
        struct j_file_extent_key
        {
          j_key_t hdr;
          uint64_t logical_addr;
        }
        */
        if (pair.Key.Size() != 16)
          return S_FALSE;
        // The offset within the file's data, in bytes, for the data stored in this extent
        const UInt64 logical_addr = Get64(p + 8);

        j_file_extent_val eval;
        if (pair.Val.Size() != k_SizeOf_j_file_extent_val)
          return S_FALSE;
        eval.Parse(pair.Val);

        if (logical_addr != 0)
        {
          PRF(printf("  logical_addr = %d", (unsigned)logical_addr));
        }
        PRF(printf("  len = %8d  pos = %8d",
            (unsigned)eval.len_and_flags,
            (unsigned)eval.phys_block_num
            ));

        CExtent ext;
        ext.logical_offset = logical_addr;
        ext.len_and_flags = eval.len_and_flags;
        ext.phys_block_num = eval.phys_block_num;
        
        if (vol.NodeIDs.IsEmpty())
          return S_FALSE;
        if (vol.NodeIDs.Back() != id)
        {
          // extents for Attributs;
          if (vol.SmallNodeIDs.IsEmpty() ||
              vol.SmallNodeIDs.Back() != id)
          {
            vol.SmallNodeIDs.Add(id);
            vol.SmallNodes.AddNew();
          }
          vol.SmallNodes.Back().Extents.Add(ext);
          continue;
          // return S_FALSE;
        }
        
        CNode &inode = vol.Nodes.Back();
        inode.Extents.Add(ext);
        continue;
      }

      if (type == APFS_TYPE_DIR_REC)
      {
        UInt32 len;
        unsigned nameOffset;

        if (isHashed)
        {
          /*
          struct j_drec_hashed_key
          {
            j_key_t hdr;
            UInt32 name_len_and_hash;
            uint8_t name[0];
          }
          */
          nameOffset = 8 + 4;
          if (pair.Key.Size() < nameOffset + 1)
            return S_FALSE;
          const UInt32 name_len_and_hash = Get32(p + 8);
          len = name_len_and_hash & J_DREC_LEN_MASK;
        }
        else
        {
          /*
          struct j_drec_key
          {
            j_key_t hdr;
            UInt16 name_len; // The length of the name, including the final null character
            uint8_t name[0]; // The name, represented as a null-terminated UTF-8 string
          }
          */
          nameOffset = 8 + 2;
          if (pair.Key.Size() < nameOffset + 1)
            return S_FALSE;
          len = Get16(p + 8);
        }
        if (nameOffset + len != pair.Key.Size())
          return S_FALSE;
        
        // CItem item;
        item.Clear();

        item.ParentId = id;
        item.Name.SetFrom_CalcLen((const char *)p + nameOffset, len);
        if (item.Name.Len() != len - 1)
          return S_FALSE;

        if (pair.Val.Size() < k_SizeOf_j_drec_val)
          return S_FALSE;

        item.Val.Parse(pair.Val);

        if (pair.Val.Size() > k_SizeOf_j_drec_val)
        {
          // fixme: parse extra fields;
          // UnsupportedFeature = true;
          // return S_FALSE;
        }
        
        vol.Items.Add(item);

        /*
        if (!vol.NodeIDs.IsEmpty() && vol.NodeIDs.Back() == id)
          vol.Nodes.Back().NumItems++;
        */
        if (id == PRIV_DIR_INO_NUM)
          vol.NumItems_In_PrivateDir++;
        
        PRF(printf(" DIR_REC next=%6d flags=%2x %s",
            (unsigned)item.Val.file_id,
            (unsigned)item.Val.flags,
            item.Name.Ptr()));
        continue;
      }

      if (type == APFS_TYPE_FILE_INFO)
      {
        if (pair.Key.Size() != 16)
          return S_FALSE;
        // j_file_info_key
        const UInt64 info_and_lba = Get64(p + 8);
       
      #define J_FILE_INFO_LBA_MASK    0x00ffffffffffffffUL
      // #define J_FILE_INFO_TYPE_MASK   0xff00000000000000UL
      #define J_FILE_INFO_TYPE_SHIFT  56
      #define APFS_FILE_INFO_DATA_HASH 1
        
        const unsigned infoType = (unsigned)(info_and_lba >> J_FILE_INFO_TYPE_SHIFT);
        // address is a paddr_t
        const UInt64 lba = info_and_lba & J_FILE_INFO_LBA_MASK;
        // j_file_data_hash_val_t
        /* Use this field of the union if the type stored in the info_and_lba field of j_file_info_val_t is
           APFS_FILE_INFO_DATA_HASH */
        if (infoType != APFS_FILE_INFO_DATA_HASH)
          return S_FALSE;
        if (pair.Val.Size() != 3 + vol.integrity.HashSize)
          return S_FALSE;
        /*
        struct j_file_data_hash_val
        {
          UInt16 hashed_len; // The length, in blocks, of the data segment that was hashed.
          UInt8 hash_size; // must be consistent with integrity_meta_phys_t::im_hash_type
          UInt8 hash[0];
        }
        */

        const unsigned hash_size = pair.Val[2];
        if (hash_size != vol.integrity.HashSize)
          return S_FALSE;

        CHashChunk hr;
        hr.hashed_len = GetUi16(pair.Val);
        if (hr.hashed_len == 0)
          return S_FALSE;
        memcpy(hr.hash, (const Byte *)pair.Val + 3, vol.integrity.HashSize);
        // (hashed_len <= 4) : we have seen
        hr.lba = lba;
        
        PRF(printf("   FILE_INFO lba = %6x, hashed_len=%6d",
            (unsigned)lba,
            (unsigned)hr.hashed_len));
        
        if (vol.Hash_IDs.IsEmpty() || vol.Hash_IDs.Back() != id)
        {
          vol.Hash_Vectors.AddNew();
          vol.Hash_IDs.Add(id);
        }
        CStreamHashes &hashes = vol.Hash_Vectors.Back();
        if (hashes.Size() != 0)
        {
          const CHashChunk &hr_Back = hashes.Back();
          if (lba != hr_Back.lba + ((UInt64)hr_Back.hashed_len << sb.block_size_Log))
            return S_FALSE;
        }
        hashes.Add(hr);
        continue;
      }

      if (type == APFS_TYPE_SNAP_METADATA)
      {
        if (pair.Key.Size() != 8)
          return S_FALSE;
        PRF(printf(" SNAP_METADATA"));
        // continue;
      }

      /* SIBLING items are used, if there are more than one hard link to some inode
         key                                     : value
         parent_id_1  DIR_REC                    : inode_id, name_1
         parent_id_2  DIR_REC                    : inode_id, name_2
         inode_id     INODE                      : parent_id_1, name_1
         inode_id     SIBLING_LINK  sibling_id_1 : parent_id_1, name_1
         inode_id     SIBLING_LINK  sibling_id_2 : parent_id_2, name_2
         sibling_id_1 SIBLING_MAP                : inode_id
         sibling_id_2 SIBLING_MAP                : inode_id
      */
      
      if (type == APFS_TYPE_SIBLING_LINK)
      {
        if (pair.Key.Size() != 16)
          return S_FALSE;
        if (pair.Val.Size() < 10 + 1)
          return S_FALSE;
        /*
        // struct j_sibling_key
        // The sibling's unique identifier.
        // This value matches the object identifier for the sibling map record
        const UInt64 sibling_id = Get64(p + 8);
        // struct j_sibling_val
        const Byte *v = pair.Val;
        const UInt64 parent_id = Get64(v); // The object identifier for the inode that's the parent directory.
        const unsigned name_len = Get16(v + 8); // len including the final null character
        if (10 + name_len != pair.Val.Size())
          return S_FALSE;
        if (v[10 + name_len - 1] != 0)
          return S_FALSE;
        AString name ((const char *)(v + 10));
        if (name.Len() != name_len - 1)
          return S_FALSE;
        PRF(printf(" SIBLING_LINK sibling_id = %6d : parent_id=%6d %s",
            (unsigned)sibling_id, (unsigned)parent_id, name.Ptr()));
        */
        continue;
      }
      
      if (type == APFS_TYPE_SIBLING_MAP)
      {
        // struct j_sibling_map_key
        // The object identifier in the header is the sibling's unique identifier
        if (pair.Key.Size() != 8 || pair.Val.Size() != 8)
          return S_FALSE;
        /*
        // j_sibling_map_val
        // The inode number of the underlying file
        const UInt64 file_id = Get64(pair.Val);
        PRF(printf(" SIBLING_MAP : file_id = %d", (unsigned)file_id));
        */
        continue;
      }
      
      UnsupportedFeature = true;
      // return S_FALSE;
    }
    ProgressVal_NumFilesTotal += vol.Items.Size();
  }


  if (needParseAttr)
  {
    /* we read external streams for attributes
       So we can get SymLink for GetProperty(kpidSymLink) later */
    FOR_VECTOR (i, vol.Nodes)
    {
      CNode &node = vol.Nodes[i];

      FOR_VECTOR (a, node.Attrs)
      {
        CAttr &attr = node.Attrs[a];
        if (attr.Data.Size() != 0 || !attr.dstream_defined)
          continue;
        if (a == node.SymLinkIndex)
        {
          if (!attr.Is_dstream_OK_for_SymLink())
            continue;
        }
        else
        {
          if (a != node.DecmpfsIndex
              // && a != node.ResourceIndex
              )
          continue;
        }
        // we don't expect big streams here
        // largest dstream for Decmpfs attribute is (2Kib+17)
        if (attr.dstream.size > ((UInt32)1 << 16))
          continue;
        CMyComPtr<ISequentialInStream> inStream;
        const HRESULT res = GetAttrStream_dstream(OpenInStream, vol, attr, &inStream);
        if (res == S_OK && inStream)
        {
          CByteBuffer buf2;
          const size_t size = (size_t)attr.dstream.size;
          buf2.Alloc(size);
          if (ReadStream_FAIL(inStream, buf2, size) == S_OK)
            attr.Data = buf2;

          ProgressVal_Cur += size;
          if (OpenCallback)
          if (ProgressVal_Cur - ProgressVal_Prev >= (1 << 22))
          {

            RINOK(OpenCallback->SetCompleted(
                &ProgressVal_NumFilesTotal,
                &ProgressVal_Cur))
            ProgressVal_Prev = ProgressVal_Cur;
          }
        }
      }

      if (node.Has_UNCOMPRESSED_SIZE())
      if (IsViDef(node.DecmpfsIndex))
      {
        CAttr &attr = node.Attrs[node.DecmpfsIndex];
        node.CompressHeader.Parse(attr.Data, attr.Data.Size());
        
        if (node.CompressHeader.IsCorrect)
          if (node.CompressHeader.Method < sizeof(MethodsMask) * 8)
            MethodsMask |= ((UInt32)1 << node.CompressHeader.Method);

        if (node.CompressHeader.IsCorrect
            && node.CompressHeader.IsSupported
            && node.CompressHeader.UnpackSize == node.uncompressed_size)
        {
          attr.NeedShow = false;
          if (node.CompressHeader.IsMethod_Resource()
              && IsViDef(node.ResourceIndex))
            node.Attrs[node.ResourceIndex].NeedShow = false;
        }
        else
        {
          vol.UnsupportedMethod = true;
        }
      }
    }
  }

  const HRESULT res = vol.FillRefs();

  if (vol.ThereAreErrors())
    HeadersError = true;
  if (vol.UnsupportedFeature)
    UnsupportedFeature = true;
  if (vol.UnsupportedMethod)
    UnsupportedMethod = true;
  if (vol.NumAltStreams != 0)
    ThereAreAltStreams = true;
  
  return res;
}



HRESULT CVol::FillRefs()
{
  {
    Refs.Reserve(Items.Size());
    // we fill Refs[*]
    // we
    // and set Nodes[*].ItemIndex for Nodes that are directories;
    FOR_VECTOR (i, Items)
    {
      CItem &item = Items[i];
      const UInt64 id = item.Val.file_id;
      // if (item.Id == ROOT_DIR_PARENT) continue;
      /* for two root folders items
         we don't set Node.ItemIndex; */
      // so nodes
      if (id == ROOT_DIR_INO_NUM)
        continue;
      if (id == PRIV_DIR_INO_NUM)
        if (NumItems_In_PrivateDir == 0) // if (inode.NumItems == 0)
          continue;
      
      CRef ref;
      ref.ItemIndex = i;
      // ref.NodeIndex = VI_MINUS1;
      ref.ParentRefIndex = VI_MINUS1;
     #ifdef APFS_SHOW_ALT_STREAMS
      ref.AttrIndex = VI_MINUS1;
     #endif
      const int index = NodeIDs.FindInSorted(id);
      // const int index = -1; // for debug
      ref.NodeIndex = (unsigned)index;
      item.RefIndex = Refs.Size();
      Refs.Add(ref);

      if (index == -1)
      {
        NodeNotFound = true;
        continue;
        // return S_FALSE;
      }
      
      // item.iNode_Index = index;
      CNode &inode = Nodes[(unsigned)index];
      if (!item.Val.IsFlags_Unknown()
          && inode.Get_Type_From_mode() != item.Val.flags)
      {
        Refs.Back().NodeIndex = VI_MINUS1;
        WrongInodeLink = true;
        continue;
        // return S_FALSE;
      }

      const bool isDir = inode.IsDir();
      if (isDir)
      {
        if (IsViDef(inode.ItemIndex))
        {
          // hard links to dirs are not supported
          Refs.Back().NodeIndex = VI_MINUS1;
          WrongInodeLink = true;
          continue;
        }
        inode.ItemIndex = i;
      }
      inode.NumCalcedLinks++;

     #ifdef APFS_SHOW_ALT_STREAMS
      if (!isDir)
      {
        // we use alt streams only for files
        const unsigned numAttrs = inode.Attrs.Size();
        if (numAttrs != 0)
        {
          ref.ParentRefIndex = item.RefIndex;
          for (unsigned k = 0; k < numAttrs; k++)
          {
            // comment it for debug
            const CAttr &attr = inode.Attrs[k];
            if (!attr.NeedShow)
              continue;

            if (k == inode.SymLinkIndex)
              continue;
            ref.AttrIndex = k;
            NumAltStreams++;
            Refs.Add(ref);
            /*
            if (attr.dstream_defined)
            {
              const int idIndex = SmallNodeIDs.FindInSorted(attr.Id);
              if (idIndex != -1)
                SmallNodes[(unsigned)idIndex].NumLinks++; // for debug
            }
            */
          }
        }
      }
     #endif
    }
  }


  {
    // fill ghost nodes
    CRef ref;
    ref.ItemIndex = VI_MINUS1;
    ref.ParentRefIndex = VI_MINUS1;
   #ifdef APFS_SHOW_ALT_STREAMS
    ref.AttrIndex = VI_MINUS1;
   #endif
    FOR_VECTOR (i, Nodes)
    {
      if (Nodes[i].NumCalcedLinks != 0)
        continue;
      const UInt64 id = NodeIDs[i];
      if (id == ROOT_DIR_INO_NUM ||
          id == PRIV_DIR_INO_NUM)
        continue;
      ThereAreUnlinkedNodes = true;
      ref.NodeIndex = i;
      Refs.Add(ref);
    }
  }

  /* if want to create Refs for ghost data streams,
     we need additional CRef::SmallNodeIndex field */

  {
    /* all Nodes[*].ItemIndex were already filled for directory Nodes,
       except of "root" and "private-dir" Nodes. */
    
    // now we fill Items[*].ParentItemIndex and Refs[*].ParentRefIndex

    UInt64 prev_ID = (UInt64)(Int64)-1;
    unsigned prev_ParentItemIndex = VI_MINUS1;

    FOR_VECTOR (i, Items)
    {
      CItem &item = Items[i];
      const UInt64 id = item.ParentId; // it's id of parent NODE
      if (id != prev_ID)
      {
        prev_ID = id;
        prev_ParentItemIndex = VI_MINUS1;
        const int index = NodeIDs.FindInSorted(id);
        if (index == -1)
          continue;
        prev_ParentItemIndex = Nodes[(unsigned)index].ItemIndex;
      }

      if (IsViNotDef(prev_ParentItemIndex))
        continue;
      item.ParentItemIndex = prev_ParentItemIndex;
      if (IsViNotDef(item.RefIndex))
      {
        // RefIndex is not set for 2 Items (root folders)
        // but there is no node for them usually
        continue;
      }
      CRef &ref = Refs[item.RefIndex];

      /*
      // it's optional check that parent_id is set correclty
      if (IsViDef(ref.NodeIndex))
      {
        const CNode &node = Nodes[ref.NodeIndex];
        if (node.IsDir() && node.parent_id != id)
          return S_FALSE;
      }
      */

      /*
      if (id == ROOT_DIR_INO_NUM)
      {
        // ItemIndex in Node for ROOT_DIR_INO_NUM was not set bofere
        // probably unused now.
        ref.ParentRefIndex = VI_MINUS1;
      }
      else
      */
        ref.ParentRefIndex = Items[prev_ParentItemIndex].RefIndex;
    }
  }

  {
    // check for loops
    const unsigned numItems = Items.Size();
    if (numItems + 1 == 0)
      return S_FALSE;
    CUIntArr arr;
    arr.Alloc(numItems);
    {
      for (unsigned i = 0; i < numItems; i++)
        arr[i] = 0;
    }
    for (unsigned i = 0; i < numItems;)
    {
      unsigned k = i++;
      for (;;)
      {
        const unsigned a = arr[k];
        if (a != 0)
        {
          if (a == i)
            return S_FALSE;
          break;
        }
        arr[k] = i;
        k = Items[k].ParentItemIndex;
        if (IsViNotDef(k))
          break;
      }
    }
  }
  
  return S_OK;
}



Z7_class_CHandler_final:
  public IInArchive,
  public IArchiveGetRawProps,
  public IInArchiveGetStream,
  public CMyUnknownImp,
  public CDatabase
{
  Z7_IFACES_IMP_UNK_3(
      IInArchive,
      IArchiveGetRawProps,
      IInArchiveGetStream)

  CMyComPtr<IInStream> _stream;
  int FindHashIndex_for_Item(UInt32 index);
};


Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  Close();
  OpenInStream = inStream;
  OpenCallback = callback;
  RINOK(Open2())
  _stream = inStream;
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _stream.Release();
  Clear();
  return S_OK;
}


enum
{
  kpidBytesWritten = kpidUserDefined,
  kpidBytesRead,
  kpidPrimeName,
  kpidParentINode,
  kpidAddTime,
  kpidGeneration,
  kpidBsdFlags
  // kpidUncompressedSize
};

static const CStatProp kProps[] =
{
  { NULL, kpidPath, VT_BSTR },
  { NULL, kpidSize, VT_UI8 },
  { NULL, kpidPackSize, VT_UI8 },
  { NULL, kpidPosixAttrib, VT_UI4 },
  { NULL, kpidMTime, VT_FILETIME },
  { NULL, kpidCTime, VT_FILETIME },
  { NULL, kpidATime, VT_FILETIME },
  { NULL, kpidChangeTime, VT_FILETIME },
  { "Added Time", kpidAddTime, VT_FILETIME },
  { NULL, kpidMethod, VT_BSTR },
  { NULL, kpidINode, VT_UI8 },
  { NULL, kpidLinks, VT_UI4 },
  { NULL, kpidSymLink, VT_BSTR },
  { NULL, kpidUserId, VT_UI4 },
  { NULL, kpidGroupId, VT_UI4 },
  { NULL, kpidCharacts, VT_BSTR },
 #ifdef APFS_SHOW_ALT_STREAMS
  { NULL, kpidIsAltStream, VT_BOOL },
 #endif
  { "Parent iNode", kpidParentINode, VT_UI8 },
  { "Primary Name", kpidPrimeName, VT_BSTR },
  { "Generation", kpidGeneration, VT_UI4 },
  { "Written Size", kpidBytesWritten, VT_UI8 },
  { "Read Size", kpidBytesRead, VT_UI8 },
  { "BSD Flags", kpidBsdFlags, VT_UI4 }
  // , { "Uncompressed Size", kpidUncompressedSize, VT_UI8 }
};


static const Byte kArcProps[] =
{
  kpidName,
  kpidCharacts,
  kpidId,
  kpidClusterSize,
  kpidCTime,
  kpidMTime,
  kpidComment
};

IMP_IInArchive_Props_WITH_NAME
IMP_IInArchive_ArcProps


static void ApfsTimeToProp(UInt64 hfsTime, NWindows::NCOM::CPropVariant &prop)
{
  if (hfsTime == 0)
    return;
  FILETIME ft;
  UInt32 ns100;
  ApfsTimeToFileTime(hfsTime, ft, ns100);
  prop.SetAsTimeFrom_FT_Prec_Ns100(ft, k_PropVar_TimePrec_1ns, ns100);
}


Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CApfs *apfs = NULL;
  if (Vols.Size() == 1)
    apfs = &Vols[0].apfs;
  switch (propID)
  {
    case kpidPhySize:
      prop = (UInt64)sb.block_count << sb.block_size_Log;
      break;
    case kpidClusterSize: prop = (UInt32)(sb.block_size); break;
    case kpidCharacts: NHfs::MethodsMaskToProp(MethodsMask, prop); break;
    case kpidMTime:
      if (apfs)
        ApfsTimeToProp(apfs->modified_by[0].timestamp, prop);
      break;
    case kpidCTime:
      if (apfs)
        ApfsTimeToProp(apfs->formatted_by.timestamp, prop);
      break;
    case kpidIsTree: prop = true; break;
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (HeadersError) flags |= kpv_ErrorFlags_HeadersError;
      if (flags != 0)
        prop = flags;
      break;
    }
    case kpidWarningFlags:
    {
      UInt32 flags = 0;
      if (UnsupportedFeature) flags |= kpv_ErrorFlags_UnsupportedFeature;
      if (UnsupportedMethod) flags |= kpv_ErrorFlags_UnsupportedMethod;
      if (flags != 0)
        prop = flags;
      break;
    }

    case kpidName:
    {
      if (apfs)
      {
        UString s;
        AddVolInternalName_toString(s, *apfs);
        s += ".apfs";
        prop = s;
      }
      break;
    }

    case kpidId:
    {
      char s[32 + 4];
      sb.uuid.SetHex_To_str(s);
      prop = s;
      break;
    }

    case kpidComment:
    {
      UString s;
      {
        AddComment_UInt64(s, "block_size", sb.block_size);

        FOR_VECTOR (i, Vols)
        {
          if (Vols.Size() > 1)
          {
            if (i != 0)
            {
              s += "----";
              s.Add_LF();
            }
            AddComment_UInt64(s, "Volume", i + 1);
          }
          Vols[i].AddComment(s);
        }
      }
      prop = s;
      break;
    }

   #ifdef APFS_SHOW_ALT_STREAMS
    case kpidIsAltStream:
      prop = ThereAreAltStreams;
      // prop = false; // for debug
      break;
   #endif
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumRawProps(UInt32 *numProps))
{
  *numProps = 0;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetRawPropInfo(UInt32 /* index */, BSTR *name, PROPID *propID))
{
  *name = NULL;
  *propID = 0;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType))
{
  *parentType = NParentType::kDir;

  const CRef2 &ref2 = Refs2[index];
  const CVol &vol = Vols[ref2.VolIndex];
  UInt32 parentIndex = (UInt32)(Int32)-1;

  if (IsViDef(ref2.RefIndex))
  {
    const CRef &ref = vol.Refs[ref2.RefIndex];
   #ifdef APFS_SHOW_ALT_STREAMS
    if (ref.IsAltStream())
      *parentType = NParentType::kAltStream;
   #endif
    if (IsViDef(ref.ParentRefIndex))
      parentIndex = (UInt32)(ref.ParentRefIndex + vol.StartRef2Index);
    else if (index != vol.RootRef2Index && IsViDef(vol.RootRef2Index))
      parentIndex = (UInt32)vol.RootRef2Index;
  }

  *parent = parentIndex;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;
  UNUSED_VAR(index)
  UNUSED_VAR(propID)
  return S_OK;
}


static void Utf8Name_to_InterName(const AString &src, UString &dest)
{
  ConvertUTF8ToUnicode(src, dest);
  NItemName::NormalizeSlashes_in_FileName_for_OsPath(dest);
}


static void AddNodeName(UString &s, const CNode &inode, UInt64 id)
{
  s += "node";
  s.Add_UInt64(id);
  if (!inode.PrimaryName.IsEmpty())
  {
    s.Add_Dot();
    UString s2;
    Utf8Name_to_InterName(inode.PrimaryName, s2);
    s += s2;
  }
}


void CDatabase::GetItemPath(unsigned index, const CNode *inode, NWindows::NCOM::CPropVariant &path) const
{
  const unsigned kNumLevelsMax = (1 << 10);
  const unsigned kLenMax = (1 << 12);
  UString s;
  const CRef2 &ref2 = Refs2[index];
  const CVol &vol = Vols[ref2.VolIndex];
  
  if (IsViDef(ref2.RefIndex))
  {
    const CRef &ref = vol.Refs[ref2.RefIndex];
    unsigned cur = ref.ItemIndex;
    UString s2;
    if (IsViNotDef(cur))
    {
      if (inode)
        AddNodeName(s, *inode, vol.NodeIDs[ref.NodeIndex]);
    }
    else
    {
      for (unsigned i = 0;; i++)
      {
        if (i >= kNumLevelsMax || s.Len() > kLenMax)
        {
          s.Insert(0, UString("[LONG_PATH]"));
          break;
        }
        const CItem &item = vol.Items[(unsigned)cur];
        Utf8Name_to_InterName(item.Name, s2);
        // s2 += "a\\b"; // for debug
        s.Insert(0, s2);
        cur = item.ParentItemIndex;
        if (IsViNotDef(cur))
          break;
        // ParentItemIndex was not set for such items
        // if (item.ParentId == ROOT_DIR_INO_NUM) break;
        s.InsertAtFront(WCHAR_PATH_SEPARATOR);
      }
    }
   
   #ifdef APFS_SHOW_ALT_STREAMS
    if (IsViDef(ref.AttrIndex) && inode)
    {
      s += ':';
      Utf8Name_to_InterName(inode->Attrs[(unsigned)ref.AttrIndex].Name, s2);
      // s2 += "a\\b"; // for debug
      s += s2;
    }
   #endif
  }
    
  if (!vol.RootName.IsEmpty())
  {
    if (IsViDef(ref2.RefIndex))
      s.InsertAtFront(WCHAR_PATH_SEPARATOR);
    s.Insert(0, vol.RootName);
  }

  path = s;
}



Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;

  const CRef2 &ref2 = Refs2[index];
  const CVol &vol = Vols[ref2.VolIndex];

  if (IsViNotDef(ref2.RefIndex))
  {
    switch (propID)
    {
      case kpidName:
      case kpidPath:
        GetItemPath(index, NULL, prop);
        break;
      case kpidIsDir:
        prop = true;
        break;
    }
    prop.Detach(value);
    return S_OK;
  }

  const CRef &ref = vol.Refs[ref2.RefIndex];

  const CItem *item = NULL;
  if (IsViDef(ref.ItemIndex))
    item = &vol.Items[ref.ItemIndex];

  const CNode *inode = NULL;
  if (IsViDef(ref.NodeIndex))
    inode = &vol.Nodes[ref.NodeIndex];

  switch (propID)
  {
    case kpidPath:
      GetItemPath(index, inode, prop);
      break;
    case kpidPrimeName:
    {
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      if (inode && !inode->PrimaryName.IsEmpty())
      {
        UString s;
        ConvertUTF8ToUnicode(inode->PrimaryName, s);
        /*
        // for debug:
        if (inode.PrimaryName != item.Name) throw 123456;
        */
        prop = s;
      }
      break;
    }

    case kpidName:
    {
      UString s;
     #ifdef APFS_SHOW_ALT_STREAMS
      if (ref.IsAltStream())
      {
        // if (inode)
        {
          const CAttr &attr = inode->Attrs[(unsigned)ref.AttrIndex];
          ConvertUTF8ToUnicode(attr.Name, s);
        }
      }
      else
     #endif
      {
        if (item)
          ConvertUTF8ToUnicode(item->Name, s);
        else if (inode)
          AddNodeName(s, *inode, vol.NodeIDs[ref.NodeIndex]);
        else
          break;
      }
      // s += "s/1bs\\2"; // for debug:
      prop = s;
      break;
    }

    case kpidSymLink:
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      if (inode)
      {
        if (inode->IsSymLink() && IsViDef(inode->SymLinkIndex))
        {
          const CByteBuffer &buf = inode->Attrs[(unsigned)inode->SymLinkIndex].Data;
          if (buf.Size() != 0)
          {
            AString s;
            s.SetFrom_CalcLen((const char *)(const Byte *)buf, (unsigned)buf.Size());
            if (s.Len() == buf.Size() - 1)
            {
              UString u;
              ConvertUTF8ToUnicode(s, u);
              prop = u;
            }
          }
        }
      }
      break;

    case kpidSize:
      if (inode)
      {
        UInt64 size = 0;
        if (inode->GetSize(ref.GetAttrIndex(), size) ||
            !inode->IsDir())
          prop = size;
      }
      break;

    case kpidPackSize:
      if (inode)
      {
        UInt64 size;
        if (inode->GetPackSize(ref.GetAttrIndex(), size) ||
            !inode->IsDir())
          prop = size;
      }
      break;

    case kpidMethod:
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      if (inode)
      {
        if (inode->CompressHeader.IsCorrect)
          inode->CompressHeader.MethodToProp(prop);
        else if (IsViDef(inode->DecmpfsIndex))
          prop = "decmpfs";
        else if (!inode->IsDir() && !inode->dstream_defined)
        {
          if (inode->IsSymLink())
          {
            if (IsViDef(inode->SymLinkIndex))
              prop = "symlink";
          }
          // else prop = "no_dstream";
        }
      }
      break;
    
    /*
    case kpidUncompressedSize:
      if (inode && inode->Has_UNCOMPRESSED_SIZE())
        prop = inode->uncompressed_size;
      break;
    */

    case kpidIsDir:
    {
      bool isDir = false;
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      {
        if (inode)
          isDir = inode->IsDir();
        else if (item)
          isDir = item->Val.IsFlags_Dir();
      }
      prop = isDir;
      break;
    }

    case kpidPosixAttrib:
    {
      if (inode)
      {
        UInt32 mode = inode->mode;
       #ifdef APFS_SHOW_ALT_STREAMS
        if (ref.IsAltStream())
        {
          mode &= 0666; // we disable execution
          mode |= MY_LIN_S_IFREG;
        }
       #endif
        prop = (UInt32)mode;
      }
      else if (item && !item->Val.IsFlags_Unknown())
        prop = (UInt32)(item->Val.flags << 12);
      break;
    }
    
    case kpidCTime: if (inode) ApfsTimeToProp(inode->create_time, prop); break;
    case kpidMTime: if (inode) ApfsTimeToProp(inode->mod_time, prop); break;
    case kpidATime: if (inode) ApfsTimeToProp(inode->access_time, prop); break;
    case kpidChangeTime: if (inode) ApfsTimeToProp(inode->change_time, prop); break;
    case kpidAddTime: if (item) ApfsTimeToProp(item->Val.date_added, prop); break;

    case kpidBytesWritten:
       #ifdef APFS_SHOW_ALT_STREAMS
        if (!ref.IsAltStream())
       #endif
        if (inode && inode->dstream_defined)
          prop = inode->dstream.total_bytes_written;
        break;
    case kpidBytesRead:
       #ifdef APFS_SHOW_ALT_STREAMS
        if (!ref.IsAltStream())
       #endif
        if (inode && inode->dstream_defined)
          prop = inode->dstream.total_bytes_read;
        break;

   #ifdef APFS_SHOW_ALT_STREAMS
    case kpidIsAltStream:
      prop = ref.IsAltStream();
      break;
   #endif
     
    case kpidCharacts:
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      if (inode)
      {
        FLAGS_TO_PROP(g_INODE_Flags, (UInt32)inode->internal_flags, prop);
      }
      break;

    case kpidBsdFlags:
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      if (inode)
      {
        FLAGS_TO_PROP(g_INODE_BSD_Flags, inode->bsd_flags, prop);
      }
      break;

    case kpidGeneration:
     #ifdef APFS_SHOW_ALT_STREAMS
      // if (!ref.IsAltStream())
     #endif
      if (inode)
        prop = inode->write_generation_counter;
      break;

    case kpidUserId:
      if (inode)
        prop = (UInt32)inode->owner;
      break;

    case kpidGroupId:
      if (inode)
        prop = (UInt32)inode->group;
      break;

    case kpidLinks:
     #ifdef APFS_SHOW_ALT_STREAMS
      if (!ref.IsAltStream())
     #endif
      if (inode && !inode->IsDir())
        prop = (UInt32)inode->nlink;
      break;

    case kpidINode:
     #ifdef APFS_SHOW_ALT_STREAMS
      // here we can disable iNode for alt stream.
      if (!ref.IsAltStream())
     #endif
      if (IsViDef(ref.NodeIndex))
        prop = (UInt32)vol.NodeIDs[ref.NodeIndex];
      break;

    case kpidParentINode:
       #ifdef APFS_SHOW_ALT_STREAMS
        if (!ref.IsAltStream())
       #endif
      if (inode)
        prop = (UInt32)inode->parent_id;
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


UInt64 CDatabase::GetSize(const UInt32 index) const
{
  const CRef2 &ref2 = Refs2[index];
  const CVol &vol = Vols[ref2.VolIndex];
  if (IsViNotDef(ref2.RefIndex))
    return 0;
  const CRef &ref = vol.Refs[ref2.RefIndex];
  if (IsViNotDef(ref.NodeIndex))
    return 0;
  const CNode &inode = vol.Nodes[ref.NodeIndex];
  UInt64 size;
  if (inode.GetSize(ref.GetAttrIndex(), size))
    return size;
  return 0;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = Refs2.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  
  {
    UInt64 totalSize = 0;
    for (i = 0; i < numItems; i++)
    {
      const UInt32 index = allFilesMode ? i : indices[i];
      totalSize += GetSize(index);
    }
    RINOK(extractCallback->SetTotal(totalSize))
  }

  UInt64 currentTotalSize = 0, currentItemSize = 0;
  
  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  NHfs::CDecoder decoder;

  for (i = 0;; i++, currentTotalSize += currentItemSize)
  {
    lps->InSize = currentTotalSize;
    lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())

    if (i >= numItems)
      break;

    const UInt32 index = allFilesMode ? i : indices[i];
    const CRef2 &ref2 = Refs2[index];
    const CVol &vol = Vols[ref2.VolIndex];

    currentItemSize = GetSize(index);

    CMyComPtr<ISequentialOutStream> realOutStream;

    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    if (IsViNotDef(ref2.RefIndex))
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }

    const CRef &ref = vol.Refs[ref2.RefIndex];
    bool isDir = false;
    if (IsViDef(ref.NodeIndex))
      isDir = vol.Nodes[ref.NodeIndex].IsDir();
    else if (IsViDef(ref.ItemIndex))
      isDir =
        #ifdef APFS_SHOW_ALT_STREAMS
          !ref.IsAltStream() &&
        #endif
          vol.Items[ref.ItemIndex].Val.IsFlags_Dir();

    if (isDir)
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))
    int opRes = NExtract::NOperationResult::kDataError;

    if (IsViDef(ref.NodeIndex))
    {
      const CNode &inode = vol.Nodes[ref.NodeIndex];
      if (
        #ifdef APFS_SHOW_ALT_STREAMS
          !ref.IsAltStream() &&
        #endif
             !inode.dstream_defined
          && inode.Extents.IsEmpty()
          && inode.Has_UNCOMPRESSED_SIZE()
          && inode.uncompressed_size == inode.CompressHeader.UnpackSize)
      {
        if (inode.CompressHeader.IsSupported)
        {
          CMyComPtr<ISequentialInStream> inStreamFork;
          UInt64 forkSize = 0;
          const CByteBuffer *decmpfs_Data = NULL;
          
          if (inode.CompressHeader.IsMethod_Resource())
          {
            if (IsViDef(inode.ResourceIndex))
            {
              const CAttr &attr = inode.Attrs[inode.ResourceIndex];
              forkSize = attr.GetSize();
              GetAttrStream(_stream, vol, attr, &inStreamFork);
            }
          }
          else
          {
            const CAttr &attr = inode.Attrs[inode.DecmpfsIndex];
            decmpfs_Data = &attr.Data;
          }
          
          if (inStreamFork || decmpfs_Data)
          {
            const HRESULT hres = decoder.Extract(
                inStreamFork, realOutStream,
                forkSize,
                inode.CompressHeader,
                decmpfs_Data,
                currentTotalSize, extractCallback,
                opRes);
            if (hres != S_FALSE && hres != S_OK)
              return hres;
          }
        }
        else
          opRes = NExtract::NOperationResult::kUnsupportedMethod;
      }
      else
      {
        CMyComPtr<ISequentialInStream> inStream;
        if (GetStream(index, &inStream) == S_OK && inStream)
        {
          COutStreamWithHash *hashStreamSpec = NULL;
          CMyComPtr<ISequentialOutStream> hashStream;
          
          if (vol.integrity.Is_SHA256())
          {
            const int hashIndex = FindHashIndex_for_Item(index);
            if (hashIndex != -1)
            {
              hashStreamSpec = new COutStreamWithHash;
              hashStream = hashStreamSpec;
              hashStreamSpec->SetStream(realOutStream);
              hashStreamSpec->Init(&(vol.Hash_Vectors[(unsigned)hashIndex]), sb.block_size_Log);
            }
          }
          
          RINOK(copyCoder->Code(inStream,
              hashStream ? hashStream : realOutStream, NULL, NULL, progress))
          opRes = NExtract::NOperationResult::kDataError;
          if (copyCoderSpec->TotalSize == currentItemSize)
          {
            opRes = NExtract::NOperationResult::kOK;
            if (hashStream)
              if (!hashStreamSpec->FinalCheck())
                opRes = NExtract::NOperationResult::kCRCError;
          }
          else if (copyCoderSpec->TotalSize < currentItemSize)
            opRes = NExtract::NOperationResult::kUnexpectedEnd;
        }
      }
    }
    
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Refs2.Size();
  return S_OK;
}


int CHandler::FindHashIndex_for_Item(UInt32 index)
{
  const CRef2 &ref2 = Refs2[index];
  const CVol &vol = Vols[ref2.VolIndex];
  if (IsViNotDef(ref2.RefIndex))
    return -1;

  const CRef &ref = vol.Refs[ref2.RefIndex];
  if (IsViNotDef(ref.NodeIndex))
    return -1;
  const CNode &inode = vol.Nodes[ref.NodeIndex];

  unsigned attrIndex = ref.GetAttrIndex();

  if (IsViNotDef(attrIndex)
      && !inode.dstream_defined
      && inode.IsSymLink())
  {
    attrIndex = inode.SymLinkIndex;
    if (IsViNotDef(attrIndex))
      return -1;
  }

  if (IsViDef(attrIndex))
  {
    /* we have seen examples, where hash available for "com.apple.ResourceFork" stream.
       these hashes for "com.apple.ResourceFork" stream are for unpacked data.
       but the caller here needs packed data of stream. So we don't use hashes */
    return -1;
  }
  else
  {
    if (!inode.dstream_defined)
      return -1;
    const UInt64 id = vol.NodeIDs[ref.NodeIndex];
    return vol.Hash_IDs.FindInSorted(id);
  }
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  *stream = NULL;

  const CRef2 &ref2 = Refs2[index];
  const CVol &vol = Vols[ref2.VolIndex];
  if (IsViNotDef(ref2.RefIndex))
    return S_FALSE;

  const CRef &ref = vol.Refs[ref2.RefIndex];
  if (IsViNotDef(ref.NodeIndex))
    return S_FALSE;
  const CNode &inode = vol.Nodes[ref.NodeIndex];

  const CRecordVector<CExtent> *extents;
  UInt64 rem = 0;

  unsigned attrIndex = ref.GetAttrIndex();

  if (IsViNotDef(attrIndex)
      && !inode.dstream_defined
      && inode.IsSymLink())
  {
    attrIndex = inode.SymLinkIndex;
    if (IsViNotDef(attrIndex))
      return S_FALSE;
  }

  if (IsViDef(attrIndex))
  {
    const CAttr &attr = inode.Attrs[(unsigned)attrIndex];
    if (!attr.dstream_defined)
    {
      CBufInStream *streamSpec = new CBufInStream;
      CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
      streamSpec->Init(attr.Data, attr.Data.Size(), (IInArchive *)this);
      *stream = streamTemp.Detach();
      return S_OK;
    }
    const int idIndex = vol.SmallNodeIDs.FindInSorted(attr.Id);
    if (idIndex != -1)
      extents = &vol.SmallNodes[(unsigned)idIndex].Extents;
    else
    {
      const int fext_Index = vol.FEXT_NodeIDs.FindInSorted(attr.Id);
      if (fext_Index == -1)
        return S_FALSE;
      extents = &vol.FEXT_Nodes[(unsigned)fext_Index].Extents;
    }
    rem = attr.dstream.size;
  }
  else
  {
    if (IsViDef(ref.ItemIndex))
      if (vol.Items[ref.ItemIndex].Val.IsFlags_Dir())
        return S_FALSE;
    if (inode.IsDir())
      return S_FALSE;
    extents = &inode.Extents;
    if (inode.dstream_defined)
    {
      rem = inode.dstream.size;
      if (inode.Extents.Size() == 0)
      {
        const int fext_Index = vol.FEXT_NodeIDs.FindInSorted(vol.NodeIDs[ref.NodeIndex]);
        if (fext_Index != -1)
          extents = &vol.FEXT_Nodes[(unsigned)fext_Index].Extents;
      }
    }
    else
    {
      // return S_FALSE; // check it !!!  How zero size files are stored with dstream_defined?
    }
  }
  return GetStream2(_stream, extents, rem, stream);
}



HRESULT CDatabase::GetAttrStream(IInStream *apfsInStream, const CVol &vol,
    const CAttr &attr, ISequentialInStream **stream)
{
  *stream = NULL;
  if (!attr.dstream_defined)
  {
    CBufInStream *streamSpec = new CBufInStream;
    CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
    streamSpec->Init(attr.Data, attr.Data.Size(), (IInArchive *)this);
    *stream = streamTemp.Detach();
    return S_OK;
  }
  return GetAttrStream_dstream(apfsInStream, vol, attr, stream);
}


HRESULT CDatabase::GetAttrStream_dstream( IInStream *apfsInStream, const CVol &vol,
    const CAttr &attr, ISequentialInStream **stream)
{
  const CRecordVector<CExtent> *extents;
  {
    const int idIndex = vol.SmallNodeIDs.FindInSorted(attr.Id);
    if (idIndex != -1)
      extents = &vol.SmallNodes[(unsigned)idIndex].Extents;
    else
    {
      const int fext_Index = vol.FEXT_NodeIDs.FindInSorted(attr.Id);
      if (fext_Index == -1)
        return S_FALSE;
      extents = &vol.FEXT_Nodes[(unsigned)fext_Index].Extents;
    }
  }
  return GetStream2(apfsInStream, extents, attr.dstream.size, stream);
}


HRESULT CDatabase::GetStream2(
    IInStream *apfsInStream,
    const CRecordVector<CExtent> *extents, UInt64 rem,
    ISequentialInStream **stream)
{
  CExtentsStream *extentStreamSpec = new CExtentsStream();
  CMyComPtr<ISequentialInStream> extentStream = extentStreamSpec;

  UInt64 virt = 0;
  FOR_VECTOR (i, *extents)
  {
    const CExtent &e = (*extents)[i];
    if (virt != e.logical_offset)
      return S_FALSE;
    const UInt64 len = EXTENT_GET_LEN(e.len_and_flags);
    if (len == 0)
    {
      return S_FALSE;
      // continue;
    }
    if (rem == 0)
      return S_FALSE;
    UInt64 cur = len;
    if (cur > rem)
      cur = rem;
    CSeekExtent se;
    se.Phy = (UInt64)e.phys_block_num << sb.block_size_Log;
    se.Virt = virt;
    virt += cur;
    rem -= cur;
    extentStreamSpec->Extents.Add(se);
    if (rem == 0)
      if (i != extents->Size() - 1)
        return S_FALSE;
  }
  
  if (rem != 0)
    return S_FALSE;
  
  CSeekExtent se;
  se.Phy = 0;
  se.Virt = virt;
  extentStreamSpec->Extents.Add(se);
  extentStreamSpec->Stream = apfsInStream;
  extentStreamSpec->Init();
  *stream = extentStream.Detach();
  return S_OK;
}


REGISTER_ARC_I(
  "APFS", "apfs img", NULL, 0xc3,
  k_Signature,
  k_SignatureOffset,
  0,
  IsArc_APFS)

}}
