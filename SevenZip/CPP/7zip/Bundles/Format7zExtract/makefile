PROG = 7zxa.dll
DEF_FILE = ../../Archive/Archive2.def
CFLAGS = $(CFLAGS) \
  -DEXTRACT_ONLY \

COMMON_OBJS = \
  $O\CRC.obj \
  $O\CrcReg.obj \
  $O\IntToString.obj \
  $O\MyString.obj \
  $O\MyVector.obj \
  $O\NewHandler.obj \
  $O\Sha256Reg.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\Wildcard.obj \

WIN_OBJS = \
  $O\PropVariant.obj \
  $O\Synchronization.obj \
  $O\System.obj \

7ZIP_COMMON_OBJS = \
  $O\CreateCoder.obj \
  $O\CWrappers.obj \
  $O\InBuffer.obj \
  $O\FilterCoder.obj \
  $O\LimitedStreams.obj \
  $O\MethodId.obj \
  $O\MethodProps.obj \
  $O\OutBuffer.obj \
  $O\ProgressUtils.obj \
  $O\PropId.obj \
  $O\StreamBinder.obj \
  $O\StreamObjects.obj \
  $O\StreamUtils.obj \
  $O\VirtThread.obj \

AR_OBJS = \
  $O\ArchiveExports.obj \
  $O\DllExports2.obj \

AR_COMMON_OBJS = \
  $O\CoderMixer2.obj \
  $O\HandlerOut.obj \
  $O\ItemNameUtils.obj \
  $O\OutStreamWithCRC.obj \
  $O\ParseProperties.obj \

7Z_OBJS = \
  $O\7zCompressionMode.obj \
  $O\7zDecode.obj \
  $O\7zExtract.obj \
  $O\7zHandler.obj \
  $O\7zHeader.obj \
  $O\7zIn.obj \
  $O\7zProperties.obj \
  $O\7zRegister.obj \


COMPRESS_OBJS = \
  $O\CodecExports.obj \
  $O\Bcj2Coder.obj \
  $O\Bcj2Register.obj \
  $O\BcjCoder.obj \
  $O\BcjRegister.obj \
  $O\BitlDecoder.obj \
  $O\BranchMisc.obj \
  $O\BranchRegister.obj \
  $O\ByteSwap.obj \
  $O\BZip2Crc.obj \
  $O\BZip2Decoder.obj \
  $O\BZip2Register.obj \
  $O\CopyCoder.obj \
  $O\CopyRegister.obj \
  $O\DeflateDecoder.obj \
  $O\DeflateRegister.obj \
  $O\DeltaFilter.obj \
  $O\Lzma2Decoder.obj \
  $O\Lzma2Register.obj \
  $O\LzmaDecoder.obj \
  $O\LzmaRegister.obj \
  $O\LzOutWindow.obj \
  $O\PpmdDecoder.obj \
  $O\PpmdRegister.obj \

CRYPTO_OBJS = \
  $O\7zAes.obj \
  $O\7zAesRegister.obj \
  $O\MyAes.obj \
  $O\MyAesReg.obj \

C_OBJS = \
  $O\Alloc.obj \
  $O\Bcj2.obj \
  $O\Bra.obj \
  $O\Bra86.obj \
  $O\BraIA64.obj \
  $O\CpuArch.obj \
  $O\Delta.obj \
  $O\Lzma2Dec.obj \
  $O\Lzma2DecMt.obj \
  $O\LzmaDec.obj \
  $O\MtDec.obj \
  $O\Ppmd7.obj \
  $O\Ppmd7Dec.obj \
  $O\Threads.obj \

!include "../../Aes.mak"
!include "../../Crc.mak"
!include "../../LzmaDec.mak"
!include "../../Sha256.mak"

!include "../../7zip.mak"
