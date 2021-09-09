PROG = 7zz.exe
# USE_C_AES = 1
# USE_C_SHA = 1
CFLAGS = $(CFLAGS) -DPROG_VARIANT_Z

!include "../Format7zF/Arc.mak"
!include "../../UI/Console/Console.mak"

COMMON_OBJS = $(COMMON_OBJS) \
  $O\CommandLineParser.obj \
  $O\ListFileUtils.obj \
  $O\StdInStream.obj \
  $O\StdOutStream.obj \

WIN_OBJS = $(WIN_OBJS) \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileLink.obj \
  $O\FileSystem.obj \
  $O\MemoryLock.obj \
  $O\PropVariantConv.obj \
  $O\Registry.obj \
  $O\SystemInfo.obj \

7ZIP_COMMON_OBJS = $(7ZIP_COMMON_OBJS) \
  $O\FilePathAutoRename.obj \
  $O\FileStreams.obj \

!include "../../7zip.mak"
