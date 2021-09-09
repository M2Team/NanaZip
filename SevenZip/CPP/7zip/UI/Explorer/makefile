PROG = 7-zip.dll
DEF_FILE = Explorer.def
CFLAGS = $(CFLAGS) \
  -DLANG \

!IFDEF UNDER_CE
LIBS = $(LIBS) Commctrl.lib
!ELSE
LIBS = $(LIBS) htmlhelp.lib comdlg32.lib Mpr.lib Gdi32.lib
CFLAGS = $(CFLAGS) -DWIN_LONG_PATH
!ENDIF

EXPLORER_OBJS = \
  $O\DllExportsExplorer.obj \
  $O\ContextMenu.obj \
  $O\MyMessages.obj \

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

WIN_OBJS = \
  $O\DLL.obj \
  $O\ErrorMsg.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileName.obj \
  $O\MemoryLock.obj \
  $O\Menu.obj \
  $O\ProcessUtils.obj \
  $O\Registry.obj \
  $O\ResourceString.obj \
  $O\Shell.obj \
  $O\Synchronization.obj \
  $O\Window.obj \

!IFDEF UNDER_CE

WIN_OBJS = $(WIN_OBJS) \
  $O\CommonDialog.obj \

!ENDIF

WIN_CTRL_OBJS = \
  $O\Dialog.obj \
  $O\ListView.obj \

UI_COMMON_OBJS = \
  $O\ArchiveName.obj \
  $O\CompressCall.obj \
  $O\ExtractingFilePath.obj \
  $O\ZipRegistry.obj \

FM_OBJS = \
  $O\FormatUtils.obj \
  $O\HelpUtils.obj \
  $O\LangUtils.obj \
  $O\ProgramLocation.obj \
  $O\RegistryUtils.obj \

C_OBJS = \
  $O\CpuArch.obj \
  $O\Threads.obj \

!include "../../7zip.mak"
