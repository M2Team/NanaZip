PROG = 7zipInstall.exe
MY_FIXED = 1

!IFDEF _64BIT_INSTALLER
CFLAGS = $(CFLAGS) -D_64BIT_INSTALLER
!ENDIF

CFLAGS = $(CFLAGS) -D_LZMA_SIZE_OPT

CFLAGS = $(CFLAGS) \
  -D_7Z_NO_METHOD_LZMA2 \
  -D_7Z_NO_METHODS_FILTERS

MAIN_OBJS = \
  $O\7zipInstall.obj \

C_OBJS = \
  $O\7zAlloc.obj \
  $O\7zArcIn.obj \
  $O\7zBuf.obj \
  $O\7zBuf2.obj \
  $O\7zCrc.obj \
  $O\7zCrcOpt.obj \
  $O\7zFile.obj \
  $O\7zDec.obj \
  $O\7zStream.obj \
  $O\Bcj2.obj \
  $O\CpuArch.obj \
  $O\DllSecur.obj \
  $O\LzmaDec.obj \

OBJS = \
  $(MAIN_OBJS) \
  $(C_OBJS) \
  $O\resource.res

!include "../../../CPP/Build.mak"

$(MAIN_OBJS): $(*B).c
	$(COMPL_O1)
$(C_OBJS): ../../$(*B).c
	$(COMPL_O1)
