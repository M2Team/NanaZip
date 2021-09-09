# Microsoft Developer Studio Project File - Name="Explorer" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Explorer - Win32 DebugU
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Explorer.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Explorer.mak" CFG="Explorer - Win32 DebugU"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Explorer - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Explorer - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Explorer - Win32 ReleaseU" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Explorer - Win32 DebugU" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Explorer - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /D "LANG" /Yu"StdAfx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib htmlhelp.lib /nologo /dll /machine:I386 /out:"C:\Program Files\7-ZIP\7-Zip.dll" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Explorer - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /D "LANG" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib htmlhelp.lib /nologo /dll /debug /machine:I386 /out:"C:\Program Files\7-ZIP\7-Zip.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "Explorer - Win32 ReleaseU"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseU"
# PROP BASE Intermediate_Dir "ReleaseU"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseU"
# PROP Intermediate_Dir "ReleaseU"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /D "LANG" /D "_MBCS" /Yu"StdAfx.h" /FD /c
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "_UNICODE" /D "UNICODE" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /D "LANG" /Yu"StdAfx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib htmlhelp.lib /nologo /dll /machine:I386 /out:"C:\Program Files\7-ZIP\7-Zip.dll" /opt:NOWIN98
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib htmlhelp.lib /nologo /dll /machine:I386 /out:"C:\Program Files\7-ZIP\7-Zipn.dll" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Explorer - Win32 DebugU"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugU"
# PROP BASE Intermediate_Dir "DebugU"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugU"
# PROP Intermediate_Dir "DebugU"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /D "LANG" /D "_MBCS" /Yu"StdAfx.h" /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "_UNICODE" /D "UNICODE" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "EXPLORER_EXPORTS" /D "LANG" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib htmlhelp.lib /nologo /dll /debug /machine:I386 /out:"C:\Program Files\7-ZIP\7-Zip.dll" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib htmlhelp.lib /nologo /dll /debug /machine:I386 /out:"C:\Program Files\7-ZIP\7-Zip.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Explorer - Win32 Release"
# Name "Explorer - Win32 Debug"
# Name "Explorer - Win32 ReleaseU"
# Name "Explorer - Win32 DebugU"
# Begin Group "Spec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\DllExportsExplorer.cpp
# End Source File
# Begin Source File

SOURCE=.\Explorer.def
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\resource.rc
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"StdAfx.h"
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "UI Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Common\ArchiveName.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveName.h
# End Source File
# Begin Source File

SOURCE=..\Common\CompressCall.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\CompressCall.h
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractingFilePath.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractingFilePath.h
# End Source File
# Begin Source File

SOURCE=..\Common\ZipRegistry.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ZipRegistry.h
# End Source File
# End Group
# Begin Group "Engine"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ContextMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\ContextMenu.h
# End Source File
# Begin Source File

SOURCE=.\MyMessages.cpp
# End Source File
# Begin Source File

SOURCE=.\MyMessages.h
# End Source File
# End Group
# Begin Group "FileManager"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\FileManager\FormatUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\FileManager\FormatUtils.h
# End Source File
# Begin Source File

SOURCE=..\FileManager\HelpUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\FileManager\HelpUtils.h
# End Source File
# Begin Source File

SOURCE=..\FileManager\IFolder.h
# End Source File
# Begin Source File

SOURCE=..\FileManager\LangUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\FileManager\LangUtils.h
# End Source File
# Begin Source File

SOURCE=..\FileManager\ProgramLocation.cpp
# End Source File
# Begin Source File

SOURCE=..\FileManager\ProgramLocation.h
# End Source File
# Begin Source File

SOURCE=..\FileManager\RegistryUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\FileManager\RegistryUtils.h
# End Source File
# End Group
# Begin Group "C"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.h
# End Source File
# End Group
# Begin Group "Common"

# PROP Default_Filter ""
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
# Begin Group "Windows"

# PROP Default_Filter ""
# Begin Group "Control"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Windows\Control\Dialog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\Dialog.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ListView.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\ListView.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\PropertyPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Control\PropertyPage.h
# End Source File
# End Group
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

SOURCE=..\..\..\Windows\FileMapping.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileName.h
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

SOURCE=..\..\..\Windows\ProcessUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\ProcessUtils.h
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

SOURCE=..\..\..\Windows\Shell.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Shell.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Synchronization.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Window.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Window.h
# End Source File
# End Group
# Begin Source File

SOURCE=".\7-zip.dll.manifest"
# End Source File
# Begin Source File

SOURCE=.\ContextMenuFlags.h
# End Source File
# End Target
# End Project
