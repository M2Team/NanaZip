# Microsoft Developer Studio Project File - Name="Far" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Far - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Far.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Far.mak" CFG="Far - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Far - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Far - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Far - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FAR_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FAR_EXPORTS" /D "EXTERNAL_CODECS" /D "NEW_FOLDER_INTERFACE" /Yu"StdAfx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"C:\Progs\Far\Plugins\7-Zip\7-ZipFar.dll" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Far - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FAR_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Gz /MTd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "FAR_EXPORTS" /D "EXTERNAL_CODECS" /D "NEW_FOLDER_INTERFACE" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"C:\Progs\Far\Plugins\7-Zip\7-ZipFar.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Far - Win32 Release"
# Name "Far - Win32 Debug"
# Begin Group "Spec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Far.def
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
# Begin Group "Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Common\CRC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.h
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
# Begin Group "Plugin"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ExtractEngine.cpp
# End Source File
# Begin Source File

SOURCE=.\ExtractEngine.h
# End Source File
# Begin Source File

SOURCE=.\Far.cpp
# End Source File
# Begin Source File

SOURCE=.\Messages.h
# End Source File
# Begin Source File

SOURCE=.\OverwriteDialogFar.cpp
# End Source File
# Begin Source File

SOURCE=.\OverwriteDialogFar.h
# End Source File
# Begin Source File

SOURCE=.\Plugin.cpp
# End Source File
# Begin Source File

SOURCE=.\Plugin.h
# End Source File
# Begin Source File

SOURCE=.\PluginDelete.cpp
# End Source File
# Begin Source File

SOURCE=.\PluginRead.cpp
# End Source File
# Begin Source File

SOURCE=.\PluginWrite.cpp
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\UpdateCallbackFar.cpp
# End Source File
# Begin Source File

SOURCE=.\UpdateCallbackFar.h
# End Source File
# End Group
# Begin Group "Far"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\FarPlugin.h
# End Source File
# Begin Source File

SOURCE=.\FarUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\FarUtils.h
# End Source File
# Begin Source File

SOURCE=.\ProgressBox.cpp
# End Source File
# Begin Source File

SOURCE=.\ProgressBox.h
# End Source File
# End Group
# Begin Group "Windows"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Windows\Defs.h
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

SOURCE=..\..\..\Windows\FileName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\FileName.h
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

SOURCE=..\..\..\Windows\Synchronization.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Synchronization.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\TimeUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\TimeUtils.h
# End Source File
# End Group
# Begin Group "UI Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Common\ArchiveExtractCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveOpenCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveOpenCallback.h
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

SOURCE=..\Common\ExtractingFilePath.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractingFilePath.h
# End Source File
# Begin Source File

SOURCE=..\Common\ExtractMode.h
# End Source File
# Begin Source File

SOURCE=..\Common\HandlerLoader.h
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
# Begin Group "Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.h
# End Source File
# End Group
# Begin Group "7-zip Common"

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

SOURCE=..\..\..\..\C\7zCrc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrcOpt.c
# SUBTRACT CPP /YX /Yc /Yu
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
# Begin Group "Arc Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithCRC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithCRC.h
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

SOURCE=..\Common\IFileExtractCallback.h
# End Source File
# Begin Source File

SOURCE=..\FileManager\IFolder.h
# End Source File
# Begin Source File

SOURCE=..\..\IPassword.h
# End Source File
# Begin Source File

SOURCE=..\..\IProgress.h
# End Source File
# Begin Source File

SOURCE=..\..\PropID.h
# End Source File
# End Group
# End Target
# End Project
