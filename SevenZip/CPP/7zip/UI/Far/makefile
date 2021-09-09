PROG = 7-ZipFar.dll
DEF_FILE = Far.def
CFLAGS = $(CFLAGS)  \
  -DEXTERNAL_CODECS \
  -DNEW_FOLDER_INTERFACE

!IFNDEF UNDER_CE
CFLAGS = $(CFLAGS) -DWIN_LONG_PATH
!ENDIF

CURRENT_OBJS = \
  $O\ExtractEngine.obj \
  $O\FarUtils.obj \
  $O\Far.obj \
  $O\OverwriteDialogFar.obj \
  $O\Plugin.obj \
  $O\PluginCommon.obj \
  $O\PluginDelete.obj \
  $O\PluginRead.obj \
  $O\PluginWrite.obj \
  $O\ProgressBox.obj \
  $O\UpdateCallbackFar.obj \

COMMON_OBJS = \
  $O\IntToString.obj \
  $O\NewHandler.obj \
  $O\MyString.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\MyVector.obj \
  $O\UTFConvert.obj \
  $O\Wildcard.obj \

WIN_OBJS = \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileLink.obj \
  $O\FileName.obj \
  $O\PropVariant.obj \
  $O\PropVariantConv.obj \
  $O\Registry.obj \
  $O\ResourceString.obj \
  $O\Synchronization.obj \
  $O\TimeUtils.obj \

7ZIP_COMMON_OBJS = \
  $O\CreateCoder.obj \
  $O\FilePathAutoRename.obj \
  $O\FileStreams.obj \
  $O\FilterCoder.obj \
  $O\LimitedStreams.obj \
  $O\ProgressUtils.obj \
  $O\PropId.obj \
  $O\StreamObjects.obj \
  $O\StreamUtils.obj \
  $O\UniqBlocks.obj \

UI_COMMON_OBJS = \
  $O\ArchiveExtractCallback.obj \
  $O\ArchiveOpenCallback.obj \
  $O\DefaultName.obj \
  $O\EnumDirItems.obj \
  $O\ExtractingFilePath.obj \
  $O\LoadCodecs.obj \
  $O\OpenArchive.obj \
  $O\PropIDUtils.obj \
  $O\SetProperties.obj \
  $O\SortUtils.obj \
  $O\UpdateAction.obj \
  $O\UpdateCallback.obj \
  $O\UpdatePair.obj \
  $O\UpdateProduce.obj \
  $O\WorkDir.obj \
  $O\ZipRegistry.obj \

AR_COMMON_OBJS = \
  $O\OutStreamWithCRC.obj \

AGENT_OBJS = \
  $O\Agent.obj \
  $O\AgentOut.obj \
  $O\AgentProxy.obj \
  $O\ArchiveFolder.obj \
  $O\ArchiveFolderOpen.obj \
  $O\ArchiveFolderOut.obj \
  $O\UpdateCallbackAgent.obj \

COMPRESS_OBJS = \
  $O\CopyCoder.obj \

C_OBJS = \
  $O\Alloc.obj \
  $O\CpuArch.obj \
  $O\Sort.obj \
  $O\Threads.obj \

!include "../../Crc.mak"

!include "../../7zip.mak"
