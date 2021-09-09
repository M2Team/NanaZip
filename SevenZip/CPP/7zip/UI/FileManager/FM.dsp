# Microsoft Developer Studio Project File - Name="FM" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=FM - Win32 DebugU
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FM.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FM.mak" CFG="FM - Win32 DebugU"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FM - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "FM - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "FM - Win32 ReleaseU" (based on "Win32 (x86) Application")
!MESSAGE "FM - Win32 DebugU" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FM - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS" /D "SUPPORT_DEVICE_FILE" /FAcs /Yu"StdAfx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib Mpr.lib htmlhelp.lib Urlmon.lib /nologo /subsystem:windows /machine:I386 /out:"C:\Util\7zFM.exe" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "FM - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MDd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib Mpr.lib htmlhelp.lib Urlmon.lib /nologo /subsystem:windows /debug /machine:I386 /out:"C:\Util\7zFM.exe" /pdbtype:sept

!ELSEIF  "$(CFG)" == "FM - Win32 ReleaseU"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseU"
# PROP BASE Intermediate_Dir "ReleaseU"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseU"
# PROP Intermediate_Dir "ReleaseU"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"StdAfx.h" /FD /c
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib Mpr.lib htmlhelp.lib Urlmon.lib /nologo /subsystem:windows /machine:I386 /out:"C:\Util\7zFM.exe" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "FM - Win32 DebugU"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugU"
# PROP BASE Intermediate_Dir "DebugU"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugU"
# PROP Intermediate_Dir "DebugU"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"StdAfx.h" /FD /GZ /c
# ADD CPP /nologo /Gz /MDd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib Mpr.lib htmlhelp.lib Urlmon.lib /nologo /subsystem:windows /debug /machine:I386 /out:"C:\Util\7zFM.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "FM - Win32 Release"
# Name "FM - Win32 Debug"
# Name "FM - Win32 ReleaseU"
# Name "FM - Win32 DebugU"
# Begin Group "Spec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\7zipLogo.ico
# End Source File
# Begin Source File

SOURCE=.\add.bmp
# End Source File
# Begin Source File

SOURCE=.\ClassDefs.cpp
# End Source File
# Begin Source File

SOURCE=.\Copy.bmp
# End Source File
# Begin Source File

SOURCE=.\Delete.bmp
# End Source File
# Begin Source File

SOURCE=.\Extract.bmp
# End Source File
# Begin Source File

SOURCE=.\FM.ico
# End Source File
# Begin Source File

SOURCE=.\Move.bmp
# End Source File
# Begin Source File

SOURCE=.\MyWindowsNew.h
# End Source File
# Begin Source File

SOURCE=.\Parent.bmp
# End Source File
# Begin Source File

SOURCE=.\Properties.bmp
# End Source File
# Begin Source File

SOURCE=.\resource.rc
# ADD BASE RSC /l 0x419
# ADD RSC /l 0x409
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"StdAfx.h"
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\Test.bmp
# End Source File
# End Group
# Begin Group "Folders"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\AltStreamsFolder.cpp
# End Source File
# Begin Source File

SOURCE=.\AltStreamsFolder.h
# End Source File
# Begin Source File

SOURCE=.\FSDrives.cpp
# End Source File
# Begin Source File

SOURCE=.\FSDrives.h
# End Source File
# Begin Source File

SOURCE=.\FSFolder.cpp
# End Source File
# Begin Source File

SOURCE=.\FSFolder.h
# End Source File
# Begin Source File

SOURCE=.\FSFolderCopy.cpp
# End Source File
# Begin Source File

SOURCE=.\IFolder.h
# End Source File
# Begin Source File

SOURCE=.\NetFolder.cpp
# End Source File
# Begin Source File

SOURCE=.\NetFolder.h
# End Source File
# Begin Source File

SOURCE=.\RootFolder.cpp
# End Source File
# Begin Source File

SOURCE=.\RootFolder.h
# End Source File
# End Group
# Begin Group "Registry"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\RegistryAssociations.cpp
# End Source File
# Begin Source File

SOURCE=.\RegistryAssociations.h
# End Source File
# Begin Source File

SOURCE=.\RegistryPlugins.cpp
# End Source File
# Begin Source File

SOURCE=.\RegistryPlugins.h
# End Source File
# Begin Source File

SOURCE=.\RegistryUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\RegistryUtils.h
# End Source File
# Begin Source File

SOURCE=.\ViewSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\ViewSettings.h
# End Source File
# End Group
# Begin Group "Panel"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\App.cpp
# End Source File
# Begin Source File

SOURCE=.\App.h
# End Source File
# Begin Source File

SOURCE=.\AppState.h
# End Source File
# Begin Source File

SOURCE=.\EnumFormatEtc.cpp
# End Source File
# Begin Source File

SOURCE=.\EnumFormatEtc.h
# End Source File
# Begin Source File

SOURCE=.\FileFolderPluginOpen.cpp
# End Source File
# Begin Source File

SOURCE=.\FileFolderPluginOpen.h
# End Source File
# Begin Source File

SOURCE=.\Panel.cpp
# End Source File
# Begin Source File

SOURCE=.\Panel.h
# End Source File
# Begin Source File

SOURCE=.\PanelCopy.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelCrc.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelDrag.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelFolderChange.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelItemOpen.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelItems.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelKey.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelListNotify.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelOperations.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelSort.cpp
# End Source File
# Begin Source File

SOURCE=.\PanelSplitFile.cpp
# End Source File
# Begin Source File

SOURCE=.\VerCtrl.cpp
# End Source File
# End Group
# Begin Group "Dialog"

# PROP Default_Filter ""
# Begin Group "Options"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\EditPage.cpp
# End Source File
# Begin Source File

SOURCE=.\EditPage.h
# End Source File
# Begin Source File

SOURCE=.\FoldersPage.cpp
# End Source File
# Begin Source File

SOURCE=.\FoldersPage.h
# End Source File
# Begin Source File

SOURCE=.\LangPage.cpp
# End Source File
# Begin Source File

SOURCE=.\LangPage.h
# End Source File
# Begin Source File

SOURCE=.\MenuPage.cpp
# End Source File
# Begin Source File

SOURCE=.\MenuPage.h
# End Source File
# Begin Source File

SOURCE=.\OptionsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\SettingsPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SettingsPage.h
# End Source File
# Begin Source File

SOURCE=.\SystemPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SystemPage.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\AboutDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\AboutDialog.h
# End Source File
# Begin Source File

SOURCE=.\BrowseDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowseDialog.h
# End Source File
# Begin Source File

SOURCE=.\ComboDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ComboDialog.h
# End Source File
# Begin Source File

SOURCE=CopyDialog.cpp
# End Source File
# Begin Source File

SOURCE=CopyDialog.h
# End Source File
# Begin Source File

SOURCE=.\DialogSize.h
# End Source File
# Begin Source File

SOURCE=.\EditDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EditDialog.h
# End Source File
# Begin Source File

SOURCE=.\LinkDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\LinkDialog.h
# End Source File
# Begin Source File

SOURCE=.\ListViewDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ListViewDialog.h
# End Source File
# Begin Source File

SOURCE=MessagesDialog.cpp
# End Source File
# Begin Source File

SOURCE=MessagesDialog.h
# End Source File
# Begin Source File

SOURCE=OverwriteDialog.cpp
# End Source File
# Begin Source File

SOURCE=OverwriteDialog.h
# End Source File
# Begin Source File

SOURCE=.\PasswordDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\PasswordDialog.h
# End Source File
# Begin Source File

SOURCE=.\ProgressDialog2.cpp
# End Source File
# Begin Source File

SOURCE=.\ProgressDialog2.h
# End Source File
# Begin Source File

SOURCE=.\SplitDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\SplitDialog.h
# End Source File
# End Group
# Begin Group "FM Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ExtractCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\ExtractCallback.h
# End Source File
# Begin Source File

SOURCE=.\FormatUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\FormatUtils.h
# End Source File
# Begin Source File

SOURCE=.\HelpUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\HelpUtils.h
# End Source File
# Begin Source File

SOURCE=.\LangUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\LangUtils.h
# End Source File
# Begin Source File

SOURCE=.\ProgramLocation.cpp
# End Source File
# Begin Source File

SOURCE=.\ProgramLocation.h
# End Source File
# Begin Source File

SOURCE=.\UpdateCallback100.cpp
# End Source File
# Begin Source File

SOURCE=.\UpdateCallback100.h
# End Source File
# End Group
# Begin Group "7-Zip Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Common\CreateCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\CreateCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilePathAutoRename.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilePathAutoRename.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\FileStreams.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\FileStreams.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilterCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilterCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\LimitedStreams.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\LimitedStreams.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodProps.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodProps.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\ProgressUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\ProgressUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\PropId.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\StreamObjects.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\StreamObjects.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\StreamUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\StreamUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\UniqBlocks.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\UniqBlocks.h
# End Source File
# End Group
# Begin Group "C"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\C\7zTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Alloc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Alloc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\DllSecur.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\DllSecur.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sort.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.h
# End Source File
# End Group
# Begin Group "Windows"

# PROP Default_Filter ""
# Begin Group "Control"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Windows\Control\ComboBox.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ComboBox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\CommandBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Dialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Dialog.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Edit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ImageList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ListView.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ListView.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ProgressBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\PropertyPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\PropertyPage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ReBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Static.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\StatusBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ToolBar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Window2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Window2.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Windows\Clipboard.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Clipboard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\COM.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\CommonDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\CommonDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Defs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Device.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\DLL.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\DLL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ErrorMsg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ErrorMsg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileDir.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileDir.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileFind.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileFind.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileIO.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileIO.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileLink.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileMapping.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileName.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileSystem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileSystem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Handle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\MemoryGlobal.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\MemoryGlobal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\MemoryLock.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\MemoryLock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Menu.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Menu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Net.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Net.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\NtCheck.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ProcessUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ProcessUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\PropVariant.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\PropVariant.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\PropVariantConv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\PropVariantConv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Registry.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Registry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ResourceString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ResourceString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\SecurityUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\SecurityUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Shell.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Shell.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Synchronization.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Synchronization.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\System.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\System.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Thread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\TimeUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\TimeUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Window.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Window.h
# End Source File
# End Group
# Begin Group "Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Common\Common.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\ComTry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Defs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\DynamicBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Exception.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Lang.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Lang.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyCom.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyVector.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyVector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\NewHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\NewHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Random.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Random.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StringConvert.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StringConvert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StringToInt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StringToInt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\UTFConvert.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\UTFConvert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Wildcard.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Wildcard.h
# End Source File
# End Group
# Begin Group "UI"

# PROP Default_Filter ""
# Begin Group "UI Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Common\ArchiveExtractCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveName.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveName.h
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveOpenCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveOpenCallback.h
# End Source File
# Begin Source File

SOURCE=..\Common\CompressCall.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\CompressCall.h
# End Source File
# Begin Source File

SOURCE=..\Common\DefaultName.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\DefaultName.h
# End Source File
# Begin Source File

SOURCE=..\Common\DirItem.h
# End Source File
# Begin Source File

SOURCE=..\Common\EnumDirItems.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\EnumDirItems.h
# End Source File
# Begin Source File

SOURCE=..\Common\ExitCode.h
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractingFilePath.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractingFilePath.h
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractMode.h
# End Source File
# Begin Source File

SOURCE=..\Common\HashCalc.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\HashCalc.h
# End Source File
# Begin Source File

SOURCE=..\Common\IFileExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\Common\LoadCodecs.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\LoadCodecs.h
# End Source File
# Begin Source File

SOURCE=..\Common\OpenArchive.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\OpenArchive.h
# End Source File
# Begin Source File

SOURCE=..\Common\Property.h
# End Source File
# Begin Source File

SOURCE=..\Common\PropIDUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\PropIDUtils.h
# End Source File
# Begin Source File

SOURCE=..\Common\SetProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\SetProperties.h
# End Source File
# Begin Source File

SOURCE=..\Common\SortUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\SortUtils.h
# End Source File
# Begin Source File

SOURCE=..\Common\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\Common\UpdateAction.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\UpdateAction.h
# End Source File
# Begin Source File

SOURCE=..\Common\UpdateCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\UpdateCallback.h
# End Source File
# Begin Source File

SOURCE=..\Common\UpdatePair.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\UpdatePair.h
# End Source File
# Begin Source File

SOURCE=..\Common\UpdateProduce.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\UpdateProduce.h
# End Source File
# Begin Source File

SOURCE=..\Common\WorkDir.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\WorkDir.h
# End Source File
# Begin Source File

SOURCE=..\Common\ZipRegistry.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ZipRegistry.h
# End Source File
# End Group
# Begin Group "Agent"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Agent\Agent.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\Agent.h
# End Source File
# Begin Source File

SOURCE=..\Agent\AgentOut.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\AgentProxy.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\AgentProxy.h
# End Source File
# Begin Source File

SOURCE=..\Agent\ArchiveFolder.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\ArchiveFolderOpen.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\ArchiveFolderOut.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\IFolderArchive.h
# End Source File
# Begin Source File

SOURCE=..\Agent\UpdateCallbackAgent.cpp
# End Source File
# Begin Source File

SOURCE=..\Agent\UpdateCallbackAgent.h
# End Source File
# End Group
# Begin Group "Explorer"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Explorer\ContextMenu.cpp
# End Source File
# Begin Source File

SOURCE=..\Explorer\ContextMenu.h
# End Source File
# Begin Source File

SOURCE=..\Explorer\ContextMenuFlags.h
# End Source File
# Begin Source File

SOURCE=..\Explorer\RegistryContextMenu.cpp
# End Source File
# Begin Source File

SOURCE=..\Explorer\RegistryContextMenu.h
# End Source File
# End Group
# Begin Group "GUI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\GUI\HashGUI.cpp
# End Source File
# Begin Source File

SOURCE=..\GUI\HashGUI.h
# End Source File
# Begin Source File

SOURCE=..\GUI\UpdateCallbackGUI2.cpp
# End Source File
# Begin Source File

SOURCE=..\GUI\UpdateCallbackGUI2.h
# End Source File
# End Group
# End Group
# Begin Group "Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.h
# End Source File
# End Group
# Begin Group "Interface"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\IArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\ICoder.h
# End Source File
# Begin Source File

SOURCE=..\..\IDecl.h
# End Source File
# Begin Source File

SOURCE=..\..\IPassword.h
# End Source File
# Begin Source File

SOURCE=..\..\IProgress.h
# End Source File
# Begin Source File

SOURCE=..\..\IStream.h
# End Source File
# Begin Source File

SOURCE=..\..\PropID.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\7zFM.exe.manifest
# End Source File
# Begin Source File

SOURCE=.\7zipLogo.ico
# End Source File
# Begin Source File

SOURCE=.\Add2.bmp
# End Source File
# Begin Source File

SOURCE=.\Copy2.bmp
# End Source File
# Begin Source File

SOURCE=.\Delete2.bmp
# End Source File
# Begin Source File

SOURCE=.\Extract2.bmp
# End Source File
# Begin Source File

SOURCE=.\FilePlugins.cpp
# End Source File
# Begin Source File

SOURCE=.\FilePlugins.h
# End Source File
# Begin Source File

SOURCE=.\FM.cpp
# End Source File
# Begin Source File

SOURCE=.\Info.bmp
# End Source File
# Begin Source File

SOURCE=.\Info2.bmp
# End Source File
# Begin Source File

SOURCE=.\Move2.bmp
# End Source File
# Begin Source File

SOURCE=.\MyCom2.h
# End Source File
# Begin Source File

SOURCE=.\MyLoadMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\MyLoadMenu.h
# End Source File
# Begin Source File

SOURCE=.\OpenCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenCallback.h
# End Source File
# Begin Source File

SOURCE=.\PluginInterface.h
# End Source File
# Begin Source File

SOURCE=.\PluginLoader.h
# End Source File
# Begin Source File

SOURCE=.\PropertyName.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertyName.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SplitUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\SplitUtils.h
# End Source File
# Begin Source File

SOURCE=.\StringUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\StringUtils.h
# End Source File
# Begin Source File

SOURCE=.\SysIconUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\SysIconUtils.h
# End Source File
# Begin Source File

SOURCE=.\Test2.bmp
# End Source File
# Begin Source File

SOURCE=.\TextPairs.cpp
# End Source File
# Begin Source File

SOURCE=.\TextPairs.h
# End Source File
# End Target
# End Project
