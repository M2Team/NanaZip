PROG = 7z.dll
DEF_FILE = ../../Archive/Archive2.def
CFLAGS = $(CFLAGS) \
  -DEXTERNAL_CODECS \

!IFNDEF UNDER_CE
CFLAGS = $(CFLAGS) -D_7ZIP_LARGE_PAGES
!ENDIF

!include "Arc.mak"

COMPRESS_OBJS = $(COMPRESS_OBJS) \
  $O\CodecExports.obj \

AR_OBJS = $(AR_OBJS) \
  $O\ArchiveExports.obj \
  $O\DllExports2.obj \


!include "../../7zip.mak"
