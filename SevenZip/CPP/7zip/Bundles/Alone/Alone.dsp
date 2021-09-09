# Microsoft Developer Studio Project File - Name="Alone" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Alone - Win32 DebugU
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Alone.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Alone.mak" CFG="Alone - Win32 DebugU"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Alone - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Alone - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "Alone - Win32 ReleaseU" (based on "Win32 (x86) Console Application")
!MESSAGE "Alone - Win32 DebugU" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Alone - Win32 Release"

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
# ADD CPP /nologo /Gr /MT /W4 /WX /GX /O1 /I "..\..\..\\" /D "NDEBUG" /D "_MBCS" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /FAcs /Yu"StdAfx.h" /FD /c
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"c:\UTIL\7za.exe" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

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
# ADD CPP /nologo /Gz /MDd /W4 /Gm /GX /ZI /Od /I "..\..\..\\" /D "_DEBUG" /D "_MBCS" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
# SUBTRACT CPP /WX
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"c:\UTIL\7za.exe" /pdbtype:sept

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

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
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "EXCLUDE_COM" /D "NO_REGISTRY" /Yu"StdAfx.h" /FD /c
# ADD CPP /nologo /Gz /MD /W4 /WX /GX /O1 /I "..\..\..\\" /D "NDEBUG" /D "UNICODE" /D "_UNICODE" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /c
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"c:\UTIL\7za.exe" /opt:NOWIN98
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"c:\UTIL\7zan.exe" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "EXCLUDE_COM" /D "NO_REGISTRY" /D "_MBCS" /Yu"StdAfx.h" /FD /GZ /c
# ADD CPP /nologo /Gz /MDd /W4 /WX /Gm /GX /ZI /Od /I "..\..\..\\" /D "_DEBUG" /D "_UNICODE" /D "UNICODE" /D "WIN32" /D "_CONSOLE" /D "WIN_LONG_PATH" /D "_7ZIP_LARGE_PAGES" /D "SUPPORT_DEVICE_FILE" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"c:\UTIL\7za.exe" /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"c:\UTIL\7zan.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Alone - Win32 Release"
# Name "Alone - Win32 Debug"
# Name "Alone - Win32 ReleaseU"
# Name "Alone - Win32 DebugU"
# Begin Group "Console"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\Console\ArError.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\BenchCon.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\BenchCon.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\CompressionMode.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\ConsoleClose.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\ConsoleClose.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\ExtractCallbackConsole.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\ExtractCallbackConsole.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\HashCon.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\HashCon.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\List.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\List.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\Main.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\MainAr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\OpenCallbackConsole.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\OpenCallbackConsole.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\PercentPrinter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\PercentPrinter.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\UpdateCallbackConsole.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\UpdateCallbackConsole.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\UserInputUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Console\UserInputUtils.h
# End Source File
# End Group
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
# Begin Group "Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Common\AutoPtr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Buffer.h
# End Source File
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

SOURCE=..\..\..\Common\LzFindPrepare.cpp
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

SOURCE=..\..\..\Common\MyException.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyGuidDef.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyInitGuid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyLinux.h
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

SOURCE=..\..\..\Common\MyUnknown.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyVector.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyVector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyWindows.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\NewHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\NewHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Sha1Prepare.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Sha1Reg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Sha256Prepare.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\Sha256Reg.cpp
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
# Begin Source File

SOURCE=..\..\..\Common\XzCrc64Init.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\XzCrc64Reg.cpp
# End Source File
# End Group
# Begin Group "Windows"

# PROP Default_Filter ""
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

SOURCE=..\..\..\Windows\PropVariantUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\PropVariantUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Registry.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\Registry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\SecurityUtils.h
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
# End Group
# Begin Group "7zip Common"

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

SOURCE=..\..\Common\FilterCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\FilterCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\InBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\InBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\InOutTempBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\InOutTempBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\LimitedStreams.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\LimitedStreams.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\LockedStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\LockedStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\MemBlocks.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\MemBlocks.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodId.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodId.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodProps.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\MethodProps.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\OffsetStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\OffsetStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\OutBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\OutBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\OutMemStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\OutMemStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Common\ProgressMt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Common\ProgressMt.h
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

SOURCE=..\..\Common\RegisterCodec.h
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
# Begin Group "Compress"

# PROP Default_Filter ""
# Begin Group "BZip2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\BZip2Const.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Crc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Crc.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Decoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Encoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Encoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Register.cpp
# End Source File
# End Group
# Begin Group "Copy"

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
# End Group
# Begin Group "Deflate"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Deflate64Register.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateConst.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateDecoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateEncoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateRegister.cpp
# End Source File
# End Group
# Begin Group "Huffman"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\HuffmanDecoder.h
# End Source File
# End Group
# Begin Group "Implode"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\ImplodeDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ImplodeDecoder.h
# End Source File
# End Group
# Begin Group "LZMA"

# PROP Default_Filter ""
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

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaEncoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmaRegister.cpp
# End Source File
# End Group
# Begin Group "PPMd"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\PpmdDecoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdEncoder.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdZip.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdZip.h
# End Source File
# End Group
# Begin Group "RangeCoder"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\RangeCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\RangeCoderBit.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\RangeCoderBitTree.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\RangeCoderOpt.h
# End Source File
# End Group
# Begin Group "Shrink"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\ShrinkDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ShrinkDecoder.h
# End Source File
# End Group
# Begin Group "BWT"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Mtf8.h
# End Source File
# End Group
# Begin Group "LZX"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Lzx.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzxDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzxDecoder.h
# End Source File
# End Group
# Begin Group "Quantum"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\QuantumDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\QuantumDecoder.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Compress\Bcj2Coder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Bcj2Coder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Bcj2Register.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BcjCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BcjCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BcjRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BitlDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BitlDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BitlEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BitmDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BitmEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BranchMisc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BranchMisc.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BranchRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ByteSwap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ByteSwap.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeltaFilter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzOutWindow.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzOutWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\XzDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\XzDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\XzEncoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\XzEncoder.h
# End Source File
# End Group
# Begin Group "Archive"

# PROP Default_Filter ""
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
# Begin Group "tar"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Tar\TarHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarHandlerOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarHeader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarHeader.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarItem.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarOut.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarUpdate.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Tar\TarUpdate.h
# End Source File
# End Group
# Begin Group "zip"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipAddCommon.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipAddCommon.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipCompressionMode.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipHandlerOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipHeader.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipItem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipItem.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipOut.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipUpdate.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Zip\ZipUpdate.h
# End Source File
# End Group
# Begin Group "Archive Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Common\CoderLoader.h
# End Source File
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
# Begin Group "cab"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Cab\CabBlockInStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabBlockInStream.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabHeader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabHeader.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabItem.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Cab\CabRegister.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Archive\Bz2Handler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\DeflateProps.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\DeflateProps.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\GzHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\IArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\LzmaHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\SplitHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\XzHandler.cpp
# End Source File
# End Group
# Begin Group "UI Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveCommandLine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveCommandLine.h
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveExtractCallback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\UI\Common\ArchiveExtractCallback.h
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
# End Group
# Begin Group "Crypto"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Crypto\7zAes.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\7zAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\7zAesRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\HmacSha1.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\HmacSha1.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\MyAes.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\MyAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\MyAesReg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Pbkdf2HmacSha1.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Pbkdf2HmacSha1.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\RandGen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\RandGen.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Sha1Cls.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\WzAes.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\WzAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipCrypto.cpp

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O1

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# ADD CPP /O1

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipCrypto.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipStrong.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipStrong.h
# End Source File
# End Group
# Begin Group "7-zip"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\ICoder.h
# End Source File
# Begin Source File

SOURCE=..\..\IDecl.h
# End Source File
# Begin Source File

SOURCE=..\..\IMyUnknown.h
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

SOURCE=..\..\MyVersion.h
# End Source File
# Begin Source File

SOURCE=..\..\PropID.h
# End Source File
# End Group
# Begin Group "C"

# PROP Default_Filter ""
# Begin Group "Xz"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\C\Xz.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Xz.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzCrc64.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzCrc64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzCrc64Opt.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzDec.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzEnc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzIn.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrcOpt.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zStream.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zVersion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Aes.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Aes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\AesOpt.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Alloc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Alloc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bcj2.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bcj2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bcj2Enc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bra.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bra.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bra86.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\BraIA64.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\BwtSort.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\BwtSort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Compiler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Delta.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Delta.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\DllSecur.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\DllSecur.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\HuffEnc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\HuffEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\IStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFind.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFind.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindMt.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindMt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindOpt.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzHash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Dec.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Dec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2DecMt.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2DecMt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Enc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Enc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaDec.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaDec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaEnc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtCoder.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtDec.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\MtDec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7Dec.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7Enc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8Dec.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8Enc.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\RotateDefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha1.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha1.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha1Opt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha256.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

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

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.c

!IF  "$(CFG)" == "Alone - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 ReleaseU"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "Alone - Win32 DebugU"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.h
# End Source File
# End Group
# End Target
# End Project
