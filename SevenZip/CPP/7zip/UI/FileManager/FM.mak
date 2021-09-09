CFLAGS = $(CFLAGS) \
  -DLANG \
  -DNEW_FOLDER_INTERFACE \

!IFDEF UNDER_CE
LIBS = $(LIBS) ceshell.lib Commctrl.lib
!ELSE
LIBS = $(LIBS) comctl32.lib htmlhelp.lib comdlg32.lib Mpr.lib Gdi32.lib
CFLAGS = $(CFLAGS) -DWIN_LONG_PATH -DSUPPORT_DEVICE_FILE
LFLAGS = $(LFLAGS) /DELAYLOAD:mpr.dll
LIBS = $(LIBS) delayimp.lib
!ENDIF

FM_OBJS = \
  $O\App.obj \
  $O\BrowseDialog.obj \
  $O\ClassDefs.obj \
  $O\EnumFormatEtc.obj \
  $O\ExtractCallback.obj \
  $O\FileFolderPluginOpen.obj \
  $O\FilePlugins.obj \
  $O\FM.obj \
  $O\FoldersPage.obj \
  $O\FormatUtils.obj \
  $O\FSFolder.obj \
  $O\FSFolderCopy.obj \
  $O\HelpUtils.obj \
  $O\LangUtils.obj \
  $O\MenuPage.obj \
  $O\MyLoadMenu.obj \
  $O\OpenCallback.obj \
  $O\OptionsDialog.obj \
  $O\Panel.obj \
  $O\PanelCopy.obj \
  $O\PanelCrc.obj \
  $O\PanelDrag.obj \
  $O\PanelFolderChange.obj \
  $O\PanelItemOpen.obj \
  $O\PanelItems.obj \
  $O\PanelKey.obj \
  $O\PanelListNotify.obj \
  $O\PanelMenu.obj \
  $O\PanelOperations.obj \
  $O\PanelSelect.obj \
  $O\PanelSort.obj \
  $O\PanelSplitFile.obj \
  $O\ProgramLocation.obj \
  $O\PropertyName.obj \
  $O\RegistryAssociations.obj \
  $O\RegistryPlugins.obj \
  $O\RegistryUtils.obj \
  $O\RootFolder.obj \
  $O\SplitUtils.obj \
  $O\StringUtils.obj \
  $O\SysIconUtils.obj \
  $O\TextPairs.obj \
  $O\UpdateCallback100.obj \
  $O\ViewSettings.obj \
  $O\AboutDialog.obj \
  $O\ComboDialog.obj \
  $O\CopyDialog.obj \
  $O\EditDialog.obj \
  $O\EditPage.obj \
  $O\LangPage.obj \
  $O\ListViewDialog.obj \
  $O\MessagesDialog.obj \
  $O\OverwriteDialog.obj \
  $O\PasswordDialog.obj \
  $O\ProgressDialog2.obj \
  $O\SettingsPage.obj \
  $O\SplitDialog.obj \
  $O\SystemPage.obj \
  $O\VerCtrl.obj \

!IFNDEF UNDER_CE

FM_OBJS = $(FM_OBJS) \
  $O\AltStreamsFolder.obj \
  $O\FSDrives.obj \
  $O\LinkDialog.obj \
  $O\NetFolder.obj \

WIN_OBJS = $(WIN_OBJS) \
  $O\FileSystem.obj \
  $O\Net.obj \
  $O\SecurityUtils.obj \

!ENDIF

C_OBJS = $(C_OBJS) \
  $O\DllSecur.obj \

AGENT_OBJS = \
  $O\Agent.obj \
  $O\AgentOut.obj \
  $O\AgentProxy.obj \
  $O\ArchiveFolder.obj \
  $O\ArchiveFolderOpen.obj \
  $O\ArchiveFolderOut.obj \
  $O\UpdateCallbackAgent.obj \
