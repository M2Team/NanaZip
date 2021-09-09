# Microsoft Developer Studio Project File - Name="7z" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=7z - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Format7z.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Format7z.mak" CFG="7z - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "7z - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "7z - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "7z - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Gr /MT /W4 /WX /GX /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /D "NO_REGISTRY" /D "EXTERNAL_CODECS" /D "_7ZIP_LARGE_PAGES" /D "_7ZIP_ST_9" /FAcs /Yu"StdAfx.h" /FD /GF /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"C:\Util\7z.dll" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none /debug

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Gr /MTd /W4 /WX /Gm /GX /ZI /Od /I "..\..\..\..\SDK" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MY7Z_EXPORTS" /D "NO_REGISTRY" /D "EXTERNAL_CODECS" /D "_7ZIP_LARGE_PAGES" /D "_7ZIP_ST_9" /Yu"StdAfx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"C:\Util\7z.dll" /pdbtype:sept /ignore:4033
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "7z - Win32 Release"
# Name "7z - Win32 Debug"
# Begin Group "Spec"

# PROP Default_Filter ""
# Begin Group "Icons"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Icons\7z.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\arj.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\bz2.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\cab.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\cpio.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\deb.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\dmg.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\fat.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\gz.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\hfs.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\iso.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\lzh.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\lzma.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\ntfs.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\rar.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\rpm.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\split.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\squashfs.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\tar.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\vhd.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\wim.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\xar.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\xz.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\z.ico
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Icons\zip.ico
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Archive\Archive2.def
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ArchiveExports.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\CodecExports.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\DllExports2.cpp
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

SOURCE=..\..\..\Common\Common.h
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

SOURCE=..\..\..\Common\DynLimBuf.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\DynLimBuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\IntToString.h
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

SOURCE=..\..\..\Common\MyInitGuid.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyLinux.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyMap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyMap.h
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

SOURCE=..\..\..\Common\MyWindows.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyWindows.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyXml.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Common\MyXml.h
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
# Begin Group "Compress"

# PROP Default_Filter ""
# Begin Group "Bit Coder"

# PROP Default_Filter ""
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
# End Group
# Begin Group "Rar Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Rar1Decoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar1Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar2Decoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar2Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar3Decoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar3Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar3Vm.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar3Vm.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar5Decoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\Rar5Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\RarCodecsRegister.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "BZip2 Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\BZip2Const.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Crc.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Crc.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Decoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Encoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Encoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\BZip2Register.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\Mtf8.h
# End Source File
# End Group
# Begin Group "Zip Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Deflate64Register.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateConst.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateEncoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeflateRegister.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\ImplodeDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\ImplodeDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdZip.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdZip.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ShrinkDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\ShrinkDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ZlibDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\ZlibDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ZlibEncoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\ZlibEncoder.h
# End Source File
# End Group
# Begin Group "7z Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Bcj2Coder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

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

SOURCE=..\..\Compress\CopyCoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\CopyCoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\CopyRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\DeltaFilter.cpp
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
# Begin Source File

SOURCE=..\..\Compress\PpmdDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdEncoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdEncoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\PpmdRegister.cpp
# End Source File
# End Group
# Begin Group "Cab Compress"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Compress\Lzx.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzxDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzxDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\QuantumDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\QuantumDecoder.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Compress\HuffmanDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzfseDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzfseDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzhDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzhDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmsDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzmsDecoder.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzOutWindow.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\LzOutWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\Compress\XpressDecoder.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Compress\XpressDecoder.h
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
# Begin Source File

SOURCE=..\..\Compress\ZDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Compress\ZDecoder.h
# End Source File
# End Group
# Begin Group "Crypto"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Crypto\7zAes.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\7zAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\7zAesRegister.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\HmacSha1.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\HmacSha1.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\HmacSha256.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\HmacSha256.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\MyAes.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\MyAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\MyAesReg.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Pbkdf2HmacSha1.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Pbkdf2HmacSha1.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\RandGen.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\RandGen.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Rar20Crypto.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Rar20Crypto.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Rar5Aes.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Rar5Aes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\RarAes.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\RarAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\Sha1Cls.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\WzAes.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\WzAes.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipCrypto.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipCrypto.h
# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipStrong.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Crypto\ZipStrong.h
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
# Begin Group "C"

# PROP Default_Filter ""
# Begin Group "xz"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\C\Xz.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /W4
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# ADD CPP /W4 /WX
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Xz.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzCrc64.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /W4 /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# ADD CPP /W4 /WX
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzCrc64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzCrc64Opt.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzDec.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /W4
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# ADD CPP /W4 /WX
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzEnc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\XzIn.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /W4
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# ADD CPP /W4 /WX
# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\..\C\7zBuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zBuf2.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zCrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zStream.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\7zTypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Aes.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Aes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Alloc.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Alloc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bcj2.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bcj2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bcj2Enc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Blake2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Blake2s.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bra.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bra.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Bra86.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\BraIA64.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\BwtSort.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

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
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\CpuArch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Delta.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Delta.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\HuffEnc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\HuffEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFind.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFind.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindMt.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindMt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzFindOpt.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzHash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Dec.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Dec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2DecMt.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2DecMt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Enc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Lzma2Enc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaDec.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaDec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\LzmaEnc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

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

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

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

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7aDec.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7Dec.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd7Enc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8Dec.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Ppmd8Enc.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Precomp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\RotateDefs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha1.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha1.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha256.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sha256.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sort.c

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Sort.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\..\..\..\C\Threads.h
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
# Begin Source File

SOURCE=..\..\Archive\7z\7zUpdateItem.h
# End Source File
# End Group
# Begin Group "Rar"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Rar\Rar5Handler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Rar\Rar5Handler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Rar\RarHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Rar\RarHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Rar\RarHeader.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Rar\RarItem.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Rar\RarVol.h
# End Source File
# End Group
# Begin Group "Cab"

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
# Begin Group "Chm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Chm\ChmHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Chm\ChmHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Chm\ChmIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Chm\ChmIn.h
# End Source File
# End Group
# Begin Group "Archive common"

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

SOURCE=..\..\Archive\Common\OutStreamWithSha1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\OutStreamWithSha1.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\ParseProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Common\ParseProperties.h
# End Source File
# End Group
# Begin Group "Iso"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoHeader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoHeader.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoItem.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Iso\IsoRegister.cpp
# End Source File
# End Group
# Begin Group "Nsis"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisDecode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisDecode.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Nsis\NsisRegister.cpp
# End Source File
# End Group
# Begin Group "Tar"

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
# Begin Group "Zip"

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
# Begin Group "Wim"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Wim\WimHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Wim\WimHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Wim\WimHandlerOut.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Wim\WimIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Wim\WimIn.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Wim\WimRegister.cpp
# End Source File
# End Group
# Begin Group "Udf"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Archive\Udf\UdfHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Udf\UdfHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Udf\UdfIn.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Udf\UdfIn.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\Archive\ApmHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ArHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ArjHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Base64Handler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\Bz2Handler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ComHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\CpioHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\CramfsHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\DeflateProps.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\DeflateProps.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\DmgHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ElfHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ExtHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\FatHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\FlvHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\GptHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\GzHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\HandlerCont.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\HandlerCont.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\HfsHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\IArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\IhexHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\LzhHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\LzmaHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\MachoHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\MbrHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\MslzHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\MubHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\NtfsHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\PeHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\PpmdHandler.cpp

!IF  "$(CFG)" == "7z - Win32 Release"

# ADD CPP /O2
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\Archive\QcowHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\RpmHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\SplitHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\SquashfsHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\SwfHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\UefiHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\VdiHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\VhdHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\VmdkHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\XarHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\XzHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Archive\XzHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\Archive\ZHandler.cpp
# End Source File
# End Group
# Begin Group "7zip"

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
# Begin Source File

SOURCE=..\..\IStream.h
# End Source File
# Begin Source File

SOURCE=..\..\PropID.h
# End Source File
# End Group
# Begin Group "Windows"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Windows\Defs.h
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

SOURCE=..\..\..\Windows\PropVariantUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Windows\PropVariantUtils.h
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
# End Group
# Begin Group "Asm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\7zAsm.asm
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\7zCrcOpt.asm

!IF  "$(CFG)" == "7z - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\..\..\..\Asm\x86\7zCrcOpt.asm
InputName=7zCrcOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=..\..\..\..\Asm\x86\7zCrcOpt.asm
InputName=7zCrcOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -omf -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\AesOpt.asm

!IF  "$(CFG)" == "7z - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Release
InputPath=..\..\..\..\Asm\x86\AesOpt.asm
InputName=AesOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
OutDir=.\Debug
InputPath=..\..\..\..\Asm\x86\AesOpt.asm
InputName=AesOpt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -omf -WX -W3 -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\Sha1Opt.asm

!IF  "$(CFG)" == "7z - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\..\..\..\Asm\x86\Sha1Opt.asm
InputName=Sha1Opt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=..\..\..\..\Asm\x86\Sha1Opt.asm
InputName=Sha1Opt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -omf -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\Asm\x86\Sha256Opt.asm

!IF  "$(CFG)" == "7z - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=..\..\..\..\Asm\x86\Sha256Opt.asm
InputName=Sha256Opt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "7z - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=..\..\..\..\Asm\x86\Sha256Opt.asm
InputName=Sha256Opt

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml.exe -c -omf -Fo$(OutDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
