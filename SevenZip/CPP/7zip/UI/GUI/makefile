PROG = 7zG.exe
CFLAGS = $(CFLAGS) \
  -DLANG \
  -DEXTERNAL_CODECS \

!IFDEF UNDER_CE
LIBS = $(LIBS) ceshell.lib Commctrl.lib
!ELSE
LIBS = $(LIBS) comctl32.lib htmlhelp.lib comdlg32.lib gdi32.lib
CFLAGS = $(CFLAGS) -DWIN_LONG_PATH -DSUPPORT_DEVICE_FILE -D_7ZIP_LARGE_PAGES
!ENDIF

GUI_OBJS = \
  $O\BenchmarkDialog.obj \
  $O\CompressDialog.obj \
  $O\ExtractDialog.obj \
  $O\ExtractGUI.obj \
  $O\GUI.obj \
  $O\HashGUI.obj \
  $O\UpdateCallbackGUI.obj \
  $O\UpdateCallbackGUI2.obj \
  $O\UpdateGUI.obj \

COMMON_OBJS = \
  $O\CommandLineParser.obj \
  $O\CRC.obj \
  $O\IntToString.obj \
  $O\Lang.obj \
  $O\ListFileUtils.obj \
  $O\MyString.obj \
  $O\MyVector.obj \
  $O\NewHandler.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\UTFConvert.obj \
  $O\Wildcard.obj \

WIN_OBJS = \
  $O\Clipboard.obj \
  $O\CommonDialog.obj \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileLink.obj \
  $O\FileName.obj \
  $O\FileSystem.obj \
  $O\MemoryGlobal.obj \
  $O\MemoryLock.obj \
  $O\PropVariant.obj \
  $O\PropVariantConv.obj \
  $O\Registry.obj \
  $O\ResourceString.obj \
  $O\Shell.obj \
  $O\Synchronization.obj \
  $O\System.obj \
  $O\SystemInfo.obj \
  $O\TimeUtils.obj \
  $O\Window.obj \

WIN_CTRL_OBJS = \
  $O\ComboBox.obj \
  $O\Dialog.obj \
  $O\ListView.obj \

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
  $O\ArchiveCommandLine.obj \
  $O\ArchiveExtractCallback.obj \
  $O\ArchiveOpenCallback.obj \
  $O\Bench.obj \
  $O\DefaultName.obj \
  $O\EnumDirItems.obj \
  $O\Extract.obj \
  $O\ExtractingFilePath.obj \
  $O\HashCalc.obj \
  $O\LoadCodecs.obj \
  $O\OpenArchive.obj \
  $O\PropIDUtils.obj \
  $O\SetProperties.obj \
  $O\SortUtils.obj \
  $O\TempFiles.obj \
  $O\Update.obj \
  $O\UpdateAction.obj \
  $O\UpdateCallback.obj \
  $O\UpdatePair.obj \
  $O\UpdateProduce.obj \
  $O\WorkDir.obj \
  $O\ZipRegistry.obj \

AR_COMMON_OBJS = \
  $O\OutStreamWithCRC.obj \

FM_OBJS = \
  $O\EditDialog.obj \
  $O\ExtractCallback.obj \
  $O\FormatUtils.obj \
  $O\HelpUtils.obj \
  $O\LangUtils.obj \
  $O\ListViewDialog.obj \
  $O\OpenCallback.obj \
  $O\ProgramLocation.obj \
  $O\PropertyName.obj \
  $O\RegistryUtils.obj \
  $O\SplitUtils.obj \
  $O\StringUtils.obj \
  $O\OverwriteDialog.obj \
  $O\PasswordDialog.obj \
  $O\ProgressDialog2.obj \

FM_OBJS = $(FM_OBJS) \
  $O\BrowseDialog.obj \
  $O\ComboDialog.obj \
  $O\SysIconUtils.obj \

EXPLORER_OBJS = \
  $O\MyMessages.obj \

COMPRESS_OBJS = \
  $O\CopyCoder.obj \

C_OBJS = \
  $O\Alloc.obj \
  $O\CpuArch.obj \
  $O\DllSecur.obj \
  $O\Sort.obj \
  $O\Threads.obj \

!include "../../Crc.mak"


!include "../../7zip.mak"
