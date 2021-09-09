PROG = 7zFM.exe
CFLAGS = $(CFLAGS) \
  -DEXTERNAL_CODECS \

!include "FM.mak"

COMMON_OBJS = \
  $O\IntToString.obj \
  $O\Lang.obj \
  $O\MyString.obj \
  $O\MyVector.obj \
  $O\NewHandler.obj \
  $O\Random.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\UTFConvert.obj \
  $O\Wildcard.obj \

WIN_OBJS = $(WIN_OBJS) \
  $O\Clipboard.obj \
  $O\CommonDialog.obj \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileLink.obj \
  $O\FileName.obj \
  $O\MemoryGlobal.obj \
  $O\MemoryLock.obj \
  $O\Menu.obj \
  $O\ProcessUtils.obj \
  $O\PropVariant.obj \
  $O\PropVariantConv.obj \
  $O\Registry.obj \
  $O\ResourceString.obj \
  $O\Shell.obj \
  $O\Synchronization.obj \
  $O\System.obj \
  $O\TimeUtils.obj \
  $O\Window.obj \


WIN_CTRL_OBJS = \
  $O\ComboBox.obj \
  $O\Dialog.obj \
  $O\ListView.obj \
  $O\PropertyPage.obj \
  $O\Window2.obj \

7ZIP_COMMON_OBJS = \
  $O\CreateCoder.obj \
  $O\FilePathAutoRename.obj \
  $O\FileStreams.obj \
  $O\FilterCoder.obj \
  $O\LimitedStreams.obj \
  $O\MethodProps.obj \
  $O\ProgressUtils.obj \
  $O\PropId.obj \
  $O\StreamObjects.obj \
  $O\StreamUtils.obj \
  $O\UniqBlocks.obj \

UI_COMMON_OBJS = \
  $O\ArchiveExtractCallback.obj \
  $O\ArchiveName.obj \
  $O\ArchiveOpenCallback.obj \
  $O\CompressCall.obj \
  $O\DefaultName.obj \
  $O\EnumDirItems.obj \
  $O\ExtractingFilePath.obj \
  $O\HashCalc.obj \
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

EXPLORER_OBJS = \
  $O\ContextMenu.obj \
  $O\RegistryContextMenu.obj \

GUI_OBJS = \
  $O\HashGUI.obj \
  $O\UpdateCallbackGUI2.obj \

COMPRESS_OBJS = \
  $O\CopyCoder.obj \

C_OBJS = $(C_OBJS) \
  $O\Alloc.obj \
  $O\CpuArch.obj \
  $O\Sort.obj \
  $O\Threads.obj \

!include "../../7zip.mak"
