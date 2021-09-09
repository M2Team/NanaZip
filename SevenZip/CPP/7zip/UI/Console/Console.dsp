# Microsoft Developer Studio Project File - Name="Console" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Console - Win32 DebugU
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Console.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Console.mak" CFG="Console - Win32 DebugU"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Console - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Console - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "Console - Win32 ReleaseU" (based on "Win32 (x86) Console Application")
!MESSAGE "Console - Win32 DebugU" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Console - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Gr /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "EXTERNAL_CODECS" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /FAcs /Yu"StdAfx.h" /FD /GF /c
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"C:\UTIL\7z.exe" /OPT:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Console - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /Gr /MTd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "_MBCS" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "EXTERNAL_CODECS" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"C:\UTIL\7z.exe" /pdbtype:sept /ignore:4033
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Console - Win32 ReleaseU"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Console___Win32_ReleaseU"
# PROP BASE Intermediate_Dir "Console___Win32_ReleaseU"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseU"
# PROP Intermediate_Dir "ReleaseU"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /MD /W3 /GX /O1 /I "../../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"StdAfx.h" /FD /c
# ADD CPP /nologo /Gr /MD /W4 /WX /GX /O1 /D "NDEBUG" /D "_UNICODE" /D "UNICODE" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "EXTERNAL_CODECS" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /c
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"C:\UTIL\7z.exe"
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"C:\UTIL\7zn.exe" /OPT:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Console - Win32 DebugU"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Console___Win32_DebugU"
# PROP BASE Intermediate_Dir "Console___Win32_DebugU"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugU"
# PROP Intermediate_Dir "DebugU"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Gm /GX /ZI /Od /I "../../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"StdAfx.h" /FD /GZ /c
# ADD CPP /nologo /Gr /MTd /W4 /WX /Gm /GX /ZI /Od /D "_DEBUG" /D "_UNICODE" /D "UNICODE" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "EXTERNAL_CODECS" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"C:\UTIL\7z.exe" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"C:\UTIL\7z.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Console - Win32 Release"
# Name "Console - Win32 Debug"
# Name "Console - Win32 ReleaseU"
# Name "Console - Win32 DebugU"
# Begin Group "Spec"

# PROP Default_Filter ""
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
# Begin Group "Console"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\BenchCon.cpp
# End Source File
# Begin Source File

SOURCE=.\BenchCon.h
# End Source File
# Begin Source File

SOURCE=.\ConsoleClose.cpp
# End Source File
# Begin Source File

SOURCE=.\ConsoleClose.h
# End Source File
# Begin Source File

SOURCE=.\ExtractCallbackConsole.cpp
# End Source File
# Begin Source File

SOURCE=.\ExtractCallbackConsole.h
# End Source File
# Begin Source File

SOURCE=.\HashCon.cpp
# End Source File
# Begin Source File

SOURCE=.\HashCon.h
# End Source File
# Begin Source File

SOURCE=.\List.cpp
# End Source File
# Begin Source File

SOURCE=.\List.h
# End Source File
# Begin Source File

SOURCE=.\Main.cpp
# End Source File
# Begin Source File

SOURCE=.\MainAr.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenCallbackConsole.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenCallbackConsole.h
# End Source File
# Begin Source File

SOURCE=.\PercentPrinter.cpp
# End Source File
# Begin Source File

SOURCE=.\PercentPrinter.h
# End Source File
# Begin Source File

SOURCE=.\UpdateCallbackConsole.cpp
# End Source File
# Begin Source File

SOURCE=.\UpdateCallbackConsole.h
# End Source File
# Begin Source File

SOURCE=.\UserInputUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\UserInputUtils.h
# End Source File
# End Group
# Begin Group "Windows"

# PROP Default_Filter ""
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

SOURCE=..\..\..\Windows\MemoryLock.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\MemoryLock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\NtCheck.h
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

SOURCE=..\..\..\Common\Defs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\ListFileUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\ListFileUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyBuffer2.h
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

SOURCE=..\..\..\Common\StdInStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StdInStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StdOutStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\StdOutStream.h
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
# Begin Group "UI Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Common\ArchiveCommandLine.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\ArchiveCommandLine.h
# End Source File
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

SOURCE=..\Common\Bench.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\Bench.h
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

SOURCE=..\Common\Extract.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\Extract.h
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

SOURCE=..\Common\TempFiles.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\TempFiles.h
# End Source File
# Begin Source File

SOURCE=..\Common\Update.cpp
# End Source File
# Begin Source File

SOURCE=..\Common\Update.h
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

SOURCE=..\Common\ZipRegistry.h
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

SOURCE=..\..\Common\RegisterArc.h
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
# Begin Group "Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.h
# End Source File
# End Group
# Begin Group "C"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.c

!IF  "$(CFG)" == "Console - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Console - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Console - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Console - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.h
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
# Begin Group "ArchiveCommon"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithCRC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithCRC.h
# End Source File
# End Group
# Begin Group "Asm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\7zAsm.asm
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\7zCrcOpt.asm

!IF  "$(CFG)" == "Console - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\..\..\..\Asm\x86\7zCrcOpt.asm
InputName=7zCrcOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Console - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=..\..\..\..\Asm\x86\7zCrcOpt.asm
InputName=7zCrcOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -omf -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Console - Win32 ReleaseU"

# Begin Custom Build
OutDir=.\ReleaseU
InputPath=..\..\..\..\Asm\x86\7zCrcOpt.asm
InputName=7zCrcOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "Console - Win32 DebugU"

# Begin Custom Build
OutDir=.\DebugU
InputPath=..\..\..\..\Asm\x86\7zCrcOpt.asm
InputName=7zCrcOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -omf -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

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
# End Target
# End Project
