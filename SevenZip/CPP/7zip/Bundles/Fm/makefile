PROG = 7zFM.exe

!include "../Format7zF/Arc.mak"

!include "../../UI/FileManager/FM.mak"

COMMON_OBJS = $(COMMON_OBJS) \
  $O\CommandLineParser.obj \
  $O\Lang.obj \
  $O\ListFileUtils.obj \
  $O\Random.obj \

WIN_OBJS = $(WIN_OBJS) \
  $O\Clipboard.obj \
  $O\CommonDialog.obj \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileLink.obj \
  $O\MemoryGlobal.obj \
  $O\MemoryLock.obj \
  $O\Menu.obj \
  $O\ProcessUtils.obj \
  $O\PropVariantConv.obj \
  $O\Registry.obj \
  $O\ResourceString.obj \
  $O\SystemInfo.obj \
  $O\Shell.obj \
  $O\Window.obj \

WIN_CTRL_OBJS = \
  $O\ComboBox.obj \
  $O\Dialog.obj \
  $O\ListView.obj \
  $O\PropertyPage.obj \
  $O\Window2.obj \

7ZIP_COMMON_OBJS = $(7ZIP_COMMON_OBJS) \
  $O\FilePathAutoRename.obj \
  $O\FileStreams.obj \

UI_COMMON_OBJS = \
  $O\ArchiveExtractCallback.obj \
  $O\ArchiveName.obj \
  $O\ArchiveOpenCallback.obj \
  $O\Bench.obj \
  $O\CompressCall2.obj \
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

EXPLORER_OBJS = \
  $O\ContextMenu.obj \
  $O\MyMessages.obj \
  $O\RegistryContextMenu.obj \

GUI_OBJS = \
  $O\BenchmarkDialog.obj \
  $O\CompressDialog.obj \
  $O\ExtractDialog.obj \
  $O\ExtractGUI.obj \
  $O\HashGUI.obj \
  $O\UpdateCallbackGUI.obj \
  $O\UpdateCallbackGUI2.obj \
  $O\UpdateGUI.obj \


!include "../../7zip.mak"
