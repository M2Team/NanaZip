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
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS_2" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /c
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
# ADD CPP /nologo /Gz /MDd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS_2" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
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
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS_2" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /c
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
# ADD CPP /nologo /Gz /MDd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "LANG" /D "WIN_LONG_PATH" /D "NEW_FOLDER_INTERFACE" /D "EXTERNAL_CODECS_2" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
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

SOURCE=..\..\UI\FileManager\7zipLogo.ico
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\add.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ClassDefs.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Copy.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Delete.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Extract.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FM.ico
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Move.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Parent.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Properties.bmp
# End Source File
# Begin Source File

SOURCE=.\resource.rc
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\StdAfx.cpp
# ADD CPP /Yc"StdAfx.h"
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Test.bmp
# End Source File
# End Group
# Begin Group "Archive"

# PROP Default_Filter ""
# Begin Group "Archive Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Common\CoderMixer2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\CoderMixer2.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\DummyOutStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\DummyOutStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\FindSignature.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\FindSignature.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\HandlerOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\HandlerOut.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\InStreamWithCRC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\InStreamWithCRC.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\ItemNameUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\ItemNameUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\MultiStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\MultiStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithCRC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithCRC.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\ParseProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\ParseProperties.h
# End Source File
# End Group
# Begin Group "7z"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\7z\7zCompressionMode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zCompressionMode.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zDecode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zDecode.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zEncode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zEncode.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zExtract.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zFolderInStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zFolderInStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zHandlerOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zHeader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zHeader.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zItem.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zOut.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zProperties.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zSpecStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zSpecStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zUpdate.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\7z\7zUpdate.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Archive\IArchive.h
# End Source File
# End Group
# Begin Group "Folders"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\FileManager\AltStreamsFolder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\AltStreamsFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FSDrives.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FSDrives.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FSFolder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FSFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FSFolderCopy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\IFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\NetFolder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\NetFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RootFolder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RootFolder.h
# End Source File
# End Group
# Begin Group "Registry"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\FileManager\RegistryAssociations.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RegistryAssociations.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RegistryPlugins.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RegistryPlugins.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RegistryUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\RegistryUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ViewSettings.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ViewSettings.h
# End Source File
# End Group
# Begin Group "Panel"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\FileManager\App.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\App.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\AppState.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\EnumFormatEtc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\EnumFormatEtc.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FileFolderPluginOpen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FileFolderPluginOpen.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Panel.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Panel.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelCopy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelCrc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelDrag.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelFolderChange.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelItemOpen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelItems.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelKey.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelListNotify.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelMenu.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelOperations.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelSelect.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelSort.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PanelSplitFile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\VerCtrl.cpp
# End Source File
# End Group
# Begin Group "Dialog"

# PROP Default_Filter ""
# Begin Group "Options"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\FileManager\EditPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\EditPage.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FoldersPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FoldersPage.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\LangPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\LangPage.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MenuPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MenuPage.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\OptionsDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SettingsPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SettingsPage.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SystemPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SystemPage.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\UI\FileManager\AboutDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\AboutDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\BrowseDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\BrowseDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ComboDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ComboDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\CopyDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\CopyDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\DialogSize.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\EditDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\EditDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\LinkDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\LinkDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ListViewDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ListViewDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MessagesDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MessagesDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\OverwriteDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\OverwriteDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PasswordDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PasswordDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ProgressDialog2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ProgressDialog2.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SplitDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SplitDialog.h
# End Source File
# End Group
# Begin Group "FM Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\FileManager\ExtractCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FormatUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FormatUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\HelpUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\HelpUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\LangUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\LangUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ProgramLocation.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\ProgramLocation.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\UpdateCallback100.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\UpdateCallback100.h
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

SOURCE=..\..\Common\CWrappers.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\CWrappers.h
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

SOURCE=..\..\Common\InOutTempBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\InOutTempBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodProps.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodProps.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\OutBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\OutBuffer.h
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

SOURCE=..\..\Common\StreamBinder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\StreamBinder.h
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
# Begin Source File

SOURCE=..\..\Common\VirtThread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\VirtThread.h
# End Source File
# End Group
# Begin Group "C"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrcOpt.c

!IF  "$(CFG)" == "FM - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
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

!IF  "$(CFG)" == "FM - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

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

SOURCE=..\..\..\..\C\LzFind.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFind.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindMt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindMt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindOpt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Dec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Dec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2DecMt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2DecMt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Enc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Enc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaDec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaDec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaEnc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtCoder.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtDec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtDec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha256.c

!IF  "$(CFG)" == "FM - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "FM - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha256.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha256Opt.c
# SUBTRACT CPP /YX /Yc /Yu
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

SOURCE=..\..\..\Windows\Control\Trackbar.h
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

SOURCE=..\..\..\Windows\SystemInfo.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\SystemInfo.h
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

SOURCE=..\..\..\Common\CommandLineParser.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\CommandLineParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Common.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\ComTry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\CRC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\CrcReg.cpp
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

SOURCE=..\..\Common\LimitedStreams.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\LimitedStreams.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\ListFileUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\ListFileUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\LzFindPrepare.cpp
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

SOURCE=..\..\..\Common\Sha256Prepare.cpp
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

SOURCE=..\..\..\Common\Types.h
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

SOURCE=..\..\UI\Common\ArchiveExtractCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveName.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveOpenCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveOpenCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Bench.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Bench.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\CompressCall.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\CompressCall2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\DefaultName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\DefaultName.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\DirItem.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\EnumDirItems.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\EnumDirItems.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ExitCode.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Extract.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Extract.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ExtractingFilePath.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ExtractingFilePath.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ExtractMode.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilterCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilterCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\HashCalc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\HashCalc.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\IFileExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\LoadCodecs.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\LoadCodecs.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\OpenArchive.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\OpenArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Property.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\PropIDUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\PropIDUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\SetProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\SetProperties.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\SortUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\SortUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\TempFiles.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\TempFiles.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Update.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\Update.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdateAction.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdateAction.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdateCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdateCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdatePair.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdatePair.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdateProduce.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\UpdateProduce.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\WorkDir.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\WorkDir.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ZipRegistry.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ZipRegistry.h
# End Source File
# End Group
# Begin Group "Agent"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\Agent\Agent.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\Agent.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\AgentOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\AgentProxy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\AgentProxy.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\ArchiveFolder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\ArchiveFolderOpen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\ArchiveFolderOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\IFolderArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\UpdateCallbackAgent.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Agent\UpdateCallbackAgent.h
# End Source File
# End Group
# Begin Group "Explorer"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\Explorer\ContextMenu.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Explorer\ContextMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Explorer\MyMessages.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Explorer\MyMessages.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Explorer\RegistryContextMenu.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Explorer\RegistryContextMenu.h
# End Source File
# End Group
# Begin Group "GUI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\GUI\BenchmarkDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\BenchmarkDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\BenchmarkDialogRes.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\CompressDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\CompressDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\ExtractDialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\ExtractDialog.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\ExtractGUI.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\ExtractGUI.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\HashGUI.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\HashGUI.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\UpdateCallbackGUI.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\UpdateCallbackGUI.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\UpdateCallbackGUI2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\UpdateCallbackGUI2.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\UpdateGUI.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\GUI\UpdateGUI.h
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
# Begin Source File

SOURCE=..\..\Compress\CopyRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Lzma2Decoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Lzma2Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Lzma2Encoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Lzma2Encoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Lzma2Register.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaEncoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaRegister.cpp
# End Source File
# End Group
# Begin Group "Interface"

# PROP Default_Filter ""
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
# End Group
# Begin Source File

SOURCE=..\..\UI\FileManager\7zFM.exe.manifest
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\7zipLogo.ico
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Add2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Copy2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Delete2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Extract2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FilePlugins.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FilePlugins.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\FM.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Info.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Info2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Move2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MyCom2.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MyLoadMenu.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\MyLoadMenu.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\OpenCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\OpenCallback.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PluginInterface.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PluginLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PropertyName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\PropertyName.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SplitUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SplitUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\StringUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\StringUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SysIconUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\SysIconUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\Test2.bmp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\TextPairs.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\FileManager\TextPairs.h
# End Source File
# End Target
# End Project
