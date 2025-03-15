/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Codecs.Specification.Fat.h
 * PURPOSE:   Definition for the on-disk structure of the FAT series file system
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

// References
// - FastFat file system driver implementation in Windows driver samples
//   - fat.h
//   - lfn.h
// - FreeBSD 14.2.0 source code
//   - sys/fs/msdosfs/bootsect.h
//   - sys/fs/msdosfs/bpb.h

#ifndef NANAZIP_CODECS_SPECIFICATION_FAT
#define NANAZIP_CODECS_SPECIFICATION_FAT

#include <Windows.h>

// Note: All on-disk structure of the FAT series file system is little-endian.

// The following nomenclature is used to describe the FAT on-disk structure:
//   LBN - is the number of a sector relative to the start of the disk.
//   VBN - is the number of a sector relative to the start of a file, directory,
//         or allocation.
//   LBO - is a byte offset relative to the start of the disk.
//   VBO - is a byte offset relative to the start of a file, directory or
//         allocation.

// LBO is >32 bits for FAT32.
typedef LONGLONG LBO;

typedef LBO *PLBO;

typedef ULONG32 VBO;
typedef VBO *PVBO;

// *****************************************************************************
// Packed and Unpacked BIOS Parameter Block
//

// The boot sector is the first physical sector (LBN == 0) on the volume. Part
// of the sector contains a BIOS Parameter Block. The BIOS in the sector is
// packed (i.e., unaligned) so we'll supply a unpacking macro to translate a
// packed BIOS into its unpacked equivalent. The unpacked BIOS structure is
// already defined in ntioapi.h so we only need to define the packed BIOS.

/**
 * @brief Packed BIOS Parameter Block for FAT12 and FAT16.
 */
typedef struct _PACKED_BIOS_PARAMETER_BLOCK
{
    UINT8 BytesPerSector[2];
    UINT8 SectorsPerCluster;
    UINT8 ReservedSectors[2];
    UINT8 Fats;
    UINT8 RootEntries[2];
    UINT8 Sectors[2];
    UINT8 Media;
    // FAT32 if SectorsPerFat is 0.
    UINT8 SectorsPerFat[2];
    UINT8 SectorsPerTrack[2];
    UINT8 Heads[2];
    UINT8 HiddenSectors[4];
    UINT8 LargeSectors[4];
} PACKED_BIOS_PARAMETER_BLOCK, *PPACKED_BIOS_PARAMETER_BLOCK;
C_ASSERT(sizeof(PACKED_BIOS_PARAMETER_BLOCK) == 25);

/**
 * @brief Packed BIOS Parameter Block for FAT32.
 */
typedef struct _PACKED_BIOS_PARAMETER_BLOCK_EX
{
    PACKED_BIOS_PARAMETER_BLOCK Bpb;
    UINT8 LargeSectorsPerFat[4];
    // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
    // |     ActiveFat     | Reserved0 | M |         Reserved1         |
    // M: MirrorDisabled
    UINT8 ExtendedFlags[2];
    UINT8 FsVersion[2];
    UINT8 RootDirFirstCluster[4];
    UINT8 FsInfoSector[2];
    UINT8 BackupBootSector[2];
    UINT8 Reserved[12];
} PACKED_BIOS_PARAMETER_BLOCK_EX, *PPACKED_BIOS_PARAMETER_BLOCK_EX;
C_ASSERT(sizeof(PACKED_BIOS_PARAMETER_BLOCK_EX) == 53);

/**
 * @brief Boot sector for FAT12 and FAT16.
 */
typedef struct _PACKED_BOOT_SECTOR
{
    UINT8 Jump[3];
    UINT8 Oem[8];
    PACKED_BIOS_PARAMETER_BLOCK PackedBpb;
    UINT8 PhysicalDriveNumber;
    UINT8 CurrentHead;
    UINT8 Signature;
    UINT8 Id[4];
    UINT8 VolumeLabel[11];
    UINT8 SystemId[8];
} PACKED_BOOT_SECTOR, *PPACKED_BOOT_SECTOR;
C_ASSERT(sizeof(PACKED_BOOT_SECTOR) == 62);

/**
 * @brief Boot sector for FAT32.
 */
typedef struct _PACKED_BOOT_SECTOR_EX
{
    UINT8 Jump[3];
    UINT8 Oem[8];
    PACKED_BIOS_PARAMETER_BLOCK_EX PackedBpb;
    UINT8 PhysicalDriveNumber;
    UINT8 CurrentHead;
    UINT8 Signature;
    UINT8 Id[4];
    UINT8 VolumeLabel[11];
    UINT8 SystemId[8];
} PACKED_BOOT_SECTOR_EX, *PPACKED_BOOT_SECTOR_EX;;
C_ASSERT(sizeof(PACKED_BOOT_SECTOR_EX) == 90);

/**
 * @brief File system information sector for FAT32.
 */
typedef struct _PACKED_FSINFO_SECTOR
{
    // UINT32
    UINT8 SectorBeginSignature[4];
    UINT8 ExtraBootCode[480];
    // UINT32
    UINT8 FsInfoSignature[4];
    // UINT32
    UINT8 FreeClusterCount[4];
    // UINT32
    UINT8 NextFreeCluster[4];
    UINT8 Reserved[12];
    // UINT32
    UINT8 SectorEndSignature[4];
} PACKED_FSINFO_SECTOR, *PPACKED_FSINFO_SECTOR;
C_ASSERT(sizeof(PACKED_FSINFO_SECTOR) == 512);

#define FSINFO_SECTOR_BEGIN_SIGNATURE 0x41615252
#define FSINFO_SECTOR_END_SIGNATURE 0xAA550000

#define FSINFO_SIGNATURE 0x61417272

// We use the CurrentHead field for our dirty partition info.

#define FAT_BOOT_SECTOR_DIRTY 0x01
#define FAT_BOOT_SECTOR_TEST_SURFACE 0x02

// Define a Fat Entry type. This type is used when representing a fat table
// entry. It also used to be used when dealing with a fat table index and a
// count of entries, but the ensuing type casting nightmare sealed this fate.
// These other two types are represented as ULONGs.
typedef UINT32 FAT_ENTRY;

#define FAT32_ENTRY_MASK 0x0FFFFFFFUL

// We use these special index values to set the dirty info for DOS/Win9x
// compatibility.

#define FAT_CLEAN_VOLUME (~FAT32_ENTRY_MASK | 0)
#define FAT_DIRTY_VOLUME (~FAT32_ENTRY_MASK | 1)

#define FAT_DIRTY_BIT_INDEX 1

// Physically, the entry is fully set if clean, and the high bit knocked out if
// it is dirty (i.e., it is really a clean bit). This means it is different
// per-FAT size.

#define FAT_CLEAN_ENTRY (~0)

#define FAT12_DIRTY_ENTRY 0x7ff
#define FAT16_DIRTY_ENTRY 0x7fff
#define FAT32_DIRTY_ENTRY 0x7fffffff

// The following constants the are the valid Fat index values.

#define FAT_CLUSTER_AVAILABLE (FAT_ENTRY)0x00000000
#define FAT_CLUSTER_RESERVED (FAT_ENTRY)0x0ffffff0
#define FAT_CLUSTER_BAD (FAT_ENTRY)0x0ffffff7
#define FAT_CLUSTER_LAST (FAT_ENTRY)0x0fffffff

// Fat files have the following time/date structures. Note that the following
// structure is a 32 bits long but USHORT aligned.

/**
 * @brief Packed FAT time.
 */
typedef struct _PACKED_FAT_TIME
{
    // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
    // |   DoubleSeconds   |        Minute         |       Hour        |
    // UINT16
    UINT8 Time[2];
} PACKED_FAT_TIME, *PPACKED_FAT_TIME;

/**
 * @brief Packed FAT date.
 */
typedef struct _PACKED_FAT_DATE
{
    // | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
    // |        Day        |     Month     |  Year (Relative to 1980)  |
    // UINT16
    UINT8 Date[2];
} PACKED_FAT_DATE, *PPACKED_FAT_DATE;

/**
 * @brief Packed FAT time stamp.
 */
typedef struct _PACKED_FAT_TIME_STAMP
{
    PACKED_FAT_TIME Time;
    PACKED_FAT_DATE Date;
} PACKED_FAT_TIME_STAMP, *PPACKED_FAT_TIME_STAMP;

/**
 * @brief FAT files have 8 character file names and 3 character extensions.
 */
typedef UINT8 FAT8DOT3[11], *PFAT8DOT3;

/**
 * @brief The packed directory entry record exists for every file/directory on
 *        the disk except for the root directory.
 */
typedef struct _PACKED_DIRENT
{
    FAT8DOT3 FileName;
    UINT8 Attributes;
    UINT8 NtByte;
    UINT8 CreationMSec;
    PACKED_FAT_TIME_STAMP CreationTime;
    PACKED_FAT_DATE LastAccessDate;
    union
    {
        // UINT16
        UINT8 ExtendedAttributes[2];
        // UINT16
        UINT8 FirstClusterOfFileHi[2];
    };
    PACKED_FAT_TIME_STAMP LastWriteTime;
    // UINT16
    UINT8 FirstClusterOfFile[2];
    ULONG32 FileSize;
} PACKED_DIRENT, *PPACKED_DIRENT;
C_ASSERT(sizeof(PACKED_DIRENT) == 32);

// The first byte of a dirent describes the dirent. There is also a routine to
// help in deciding how to interpret the dirent.

#define FAT_DIRENT_NEVER_USED 0x00
#define FAT_DIRENT_REALLY_0E5 0x05
#define FAT_DIRENT_DIRECTORY_ALIAS 0x2e
#define FAT_DIRENT_DELETED 0xe5

// Define the NtByte bits. These two bits are used for EFS on FAT.
//   0x1 means the file contents are encrypted
//   0x2 means the EFS metadata header is big. (this optimization means we don't
//       have to read in the first sector of the file stream to get the normal
//       header size)

#define FAT_DIRENT_NT_BYTE_ENCRYPTED 0x01 
#define FAT_DIRENT_NT_BYTE_BIG_HEADER 0x02

// These two bits optimize the case in which either the name or extension are
// all lower case.

#define FAT_DIRENT_NT_BYTE_8_LOWER_CASE 0x08
#define FAT_DIRENT_NT_BYTE_3_LOWER_CASE 0x10

// Define the various dirent attributes

#define FAT_DIRENT_ATTR_READ_ONLY 0x01
#define FAT_DIRENT_ATTR_HIDDEN 0x02
#define FAT_DIRENT_ATTR_SYSTEM 0x04
#define FAT_DIRENT_ATTR_VOLUME_ID 0x08
#define FAT_DIRENT_ATTR_DIRECTORY 0x10
#define FAT_DIRENT_ATTR_ARCHIVE 0x20
#define FAT_DIRENT_ATTR_DEVICE 0x40
#define FAT_DIRENT_ATTR_LFN ( \
    FAT_DIRENT_ATTR_READ_ONLY | FAT_DIRENT_ATTR_HIDDEN | \
    FAT_DIRENT_ATTR_SYSTEM | FAT_DIRENT_ATTR_VOLUME_ID)

/**
 * @brief This strucure defines the on disk format on long file name dirents.
 */
typedef struct _PACKED_LFN_DIRENT
{
    UINT8 Ordinal;
    // Actually 5 chars, but not WCHAR aligned.
    UINT8 Name1[10];
    UINT8 Attributes;
    UINT8 Type;
    UINT8 Checksum;
    WCHAR Name2[6];
    // UINT16
    UINT8 MustBeZero[2];
    WCHAR Name3[2];
} PACKED_LFN_DIRENT, *PPACKED_LFN_DIRENT;
C_ASSERT(sizeof(PACKED_LFN_DIRENT) == 32);

// Ordinal field
#define FAT_LAST_LONG_ENTRY 0x40
// Type field
#define FAT_LONG_NAME_COMP 0x0  

// This is the largest size buffer we would ever need to read an LFN

#define MAX_LFN_CHARACTERS 260
#define MAX_LFN_DIRENTS 20

// On-disk extension for EFS files.

#define FAT_EFS_EXTENSION L".PFILE"
#define FAT_EFS_EXTENSION_CHARCOUNT (6)
#define FAT_EFS_EXTENSION_BYTECOUNT (12)

// On-disk extension for EA data.

#define EA_FILE_SIGNATURE (0x4445) // "ED"
#define EA_SET_SIGNATURE (0x4145) // "EA"

// If the volume contains any ea data then there is one EA file called
// "EA DATA. SF" located in the root directory as Hidden, System and ReadOnly.

typedef UINT8 PACKED_EA_OFFSET[2], *PPACKED_EA_OFFSET;

/**
 * @brief Packed EA file header.
 */
typedef struct _PACKED_EA_FILE_HEADER
{
    // UINT16
    UINT8 Signature[2];
    // UINT16
    UINT8 FormatType[2];
    // UINT16
    UINT8 LogType[2];
    // UINT16
    UINT8 Cluster1[2];
    // UINT16
    UINT8 NewCValue1[2];
    // UINT16
    UINT8 Cluster2[2];
    // UINT16
    UINT8 NewCValue2[2];
    // UINT16
    UINT8 Cluster3[2];
    // UINT16
    UINT8 NewCValue3[2];
    // UINT16
    UINT8 Handle[2];
    // UINT16
    UINT8 NewHOffset[2];
    UINT8 Reserved[10];
    PACKED_EA_OFFSET EaBaseTable[240];
} PACKED_EA_FILE_HEADER, *PPACKED_EA_FILE_HEADER;
C_ASSERT(sizeof(PACKED_EA_FILE_HEADER) == 512);

typedef PACKED_EA_OFFSET EA_OFFSET_TABLE[128], *PEA_OFFSET_TABLE;

/**
 * @brief Every file with an extended attribute contains in its dirent an index
 *        into the EaMapTable. The map table contains an offset within the EA
 *        file (cluster aligned) of the EA data for the file. The individual EA
 *        data for each file is prefaced with an EA Data Header.
 */
typedef struct _PACKED_EA_SET_HEADER
{
    // UINT16
    UINT8 Signature[2];
    // UINT16
    UINT8 OwnEaHandle[2];
    // UINT32
    UINT8 NeedEaCount[4];
    UINT8 OwnerFileName[14];
    UINT8 Reserved[4];
    // UINT32
    UINT8 cbList[4];
    UINT8 PackedEas[ANYSIZE_ARRAY];
} PACKED_EA_SET_HEADER, *PPACKED_EA_SET_HEADER;
#define SIZE_OF_EA_SET_HEADER 30
#define ACTUAL_SIZE_OF_EA_SET_HEADER \
    FIELD_OFFSET(PACKED_EA_SET_HEADER, PackedEas)
C_ASSERT(ACTUAL_SIZE_OF_EA_SET_HEADER == SIZE_OF_EA_SET_HEADER);

#define MAXIMUM_EA_SIZE 0x0000ffff

/**
 * @brief Every individual EA in an EA set is declared the following packed EA.
 */
typedef struct _PACKED_EA {
    UINT8 Flags;
    // The null terminator is not included in the length. But the EA name is
    // always null terminated.
    UINT8 EaNameLength;
    // UINT16
    UINT8 EaValueLength[2];
    UINT8 EaName[ANYSIZE_ARRAY];
} PACKED_EA, *PPACKED_EA;

#define EA_NEED_EA_FLAG 0x80
#define MIN_EA_HANDLE 1
#define MAX_EA_HANDLE 30719
#define UNUSED_EA_HANDLE 0xffff
#define EA_CBLIST_OFFSET 0x1a
#define MAX_EA_BASE_INDEX 240
#define MAX_EA_OFFSET_INDEX 128

#endif // !NANAZIP_CODECS_SPECIFICATION_FAT
