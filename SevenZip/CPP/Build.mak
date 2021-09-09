LIBS = $(LIBS) oleaut32.lib ole32.lib

!IFNDEF MY_NO_UNICODE
CFLAGS = $(CFLAGS) -DUNICODE -D_UNICODE
!ENDIF

!IFNDEF O
!IFDEF PLATFORM
O=$(PLATFORM)
!ELSE
O=o
!ENDIF
!ENDIF

!IF "$(CC)" != "clang-cl"
# CFLAGS = $(CFLAGS) -FAsc -Fa$O/asm/
!ENDIF


!IF "$(PLATFORM)" == "x64"
MY_ML = ml64 -WX
#-Dx64
!ELSEIF "$(PLATFORM)" == "arm"
MY_ML = armasm -WX
!ELSE
MY_ML = ml -WX
# -DABI_CDECL
!ENDIF

# MY_ML = "$(MY_ML) -Fl$O\asm\


!IFDEF UNDER_CE
RFLAGS = $(RFLAGS) -dUNDER_CE
!IFDEF MY_CONSOLE
LFLAGS = $(LFLAGS) /ENTRY:mainACRTStartup
!ENDIF
!ELSE
!IFDEF OLD_COMPILER
LFLAGS = $(LFLAGS) -OPT:NOWIN98
!ENDIF
!IF "$(PLATFORM)" != "arm" && "$(PLATFORM)" != "arm64"
CFLAGS = $(CFLAGS) -Gr
!ENDIF
LIBS = $(LIBS) user32.lib advapi32.lib shell32.lib
!ENDIF

!IF "$(PLATFORM)" == "arm"
COMPL_ASM = $(MY_ML) $** $O/$(*B).obj
!ELSE
COMPL_ASM = $(MY_ML) -c -Fo$O/ $**
!ENDIF

CFLAGS = $(CFLAGS) -nologo -c -Fo$O/ -W4 -WX -EHsc -Gy -GR- -GF

!IF "$(CC)" == "clang-cl"

CFLAGS = $(CFLAGS) \
  -Werror \
  -Wextra \
  -Wall \
  -Weverything \
  -Wno-extra-semi-stmt \
  -Wno-extra-semi \
  -Wno-zero-as-null-pointer-constant \
  -Wno-sign-conversion \
  -Wno-old-style-cast \
  -Wno-reserved-id-macro \
  -Wno-deprecated-dynamic-exception-spec \
  -Wno-language-extension-token \
  -Wno-global-constructors \
  -Wno-non-virtual-dtor \
  -Wno-deprecated-copy-dtor \
  -Wno-exit-time-destructors \
  -Wno-switch-enum \
  -Wno-covered-switch-default \
  -Wno-nonportable-system-include-path \
  -Wno-c++98-compat-pedantic \
  -Wno-cast-qual \
  -Wc++11-extensions \

!ENDIF

!IFDEF MY_DYNAMIC_LINK
CFLAGS = $(CFLAGS) -MD
!ELSE
!IFNDEF MY_SINGLE_THREAD
CFLAGS = $(CFLAGS) -MT
!ENDIF
!ENDIF


CFLAGS = $(CFLAGS_COMMON) $(CFLAGS)

!IFNDEF OLD_COMPILER
CFLAGS = $(CFLAGS) -GS- -Zc:forScope -Zc:wchar_t
!IFNDEF UNDER_CE
!IF "$(CC)" != "clang-cl"
CFLAGS = $(CFLAGS) -MP4
!ENDIF
!IFNDEF PLATFORM
# CFLAGS = $(CFLAGS) -arch:IA32
!ENDIF
!ENDIF
!ELSE
CFLAGS = $(CFLAGS)
!ENDIF

!IFDEF MY_CONSOLE
CFLAGS = $(CFLAGS) -D_CONSOLE
!ENDIF

!IFNDEF UNDER_CE
!IF "$(PLATFORM)" == "arm"
CFLAGS = $(CFLAGS) -D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE
!ENDIF
!ENDIF

!IF "$(PLATFORM)" == "x64"
CFLAGS_O1 = $(CFLAGS) -O1
!ELSE
CFLAGS_O1 = $(CFLAGS) -O1
!ENDIF
CFLAGS_O2 = $(CFLAGS) -O2

LFLAGS = $(LFLAGS) -nologo -OPT:REF -OPT:ICF

!IFNDEF UNDER_CE
LFLAGS = $(LFLAGS) /LARGEADDRESSAWARE
!ENDIF

!IFDEF DEF_FILE
LFLAGS = $(LFLAGS) -DLL -DEF:$(DEF_FILE)
!ELSE
!IF defined(MY_FIXED) && "$(PLATFORM)" != "arm" && "$(PLATFORM)" != "arm64"
LFLAGS = $(LFLAGS) /FIXED
!ELSE
LFLAGS = $(LFLAGS) /FIXED:NO
!ENDIF
# /BASE:0x400000
!ENDIF


# !IF "$(PLATFORM)" == "x64"

!IFDEF SUB_SYS_VER

MY_SUB_SYS_VER=5.02

!IFDEF MY_CONSOLE
LFLAGS = $(LFLAGS) /SUBSYSTEM:console,$(MY_SUB_SYS_VER)
!ELSE
LFLAGS = $(LFLAGS) /SUBSYSTEM:windows,$(MY_SUB_SYS_VER)
!ENDIF

!ENDIF


PROGPATH = $O\$(PROG)

COMPL_O1   = $(CC) $(CFLAGS_O1) $**
COMPL_O2   = $(CC) $(CFLAGS_O2) $**
COMPL_PCH  = $(CC) $(CFLAGS_O1) -Yc"StdAfx.h" -Fp$O/a.pch $**
COMPL      = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $**

COMPLB    = $(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $<
# COMPLB_O2 = $(CC) $(CFLAGS_O2) -Yu"StdAfx.h" -Fp$O/a.pch $<
COMPLB_O2 = $(CC) $(CFLAGS_O2) $<

CFLAGS_C_ALL = $(CFLAGS_O2) $(CFLAGS_C_SPEC)
CCOMPL_PCH  = $(CC) $(CFLAGS_C_ALL) -Yc"Precomp.h" -Fp$O/a.pch $**
CCOMPL_USE  = $(CC) $(CFLAGS_C_ALL) -Yu"Precomp.h" -Fp$O/a.pch $**
CCOMPL      = $(CC) $(CFLAGS_C_ALL) $**
CCOMPLB     = $(CC) $(CFLAGS_C_ALL) $<

!IF "$(CC)" == "clang-cl"
COMPL  = $(COMPL) -FI StdAfx.h
COMPLB = $(COMPLB) -FI StdAfx.h
CCOMPL_USE = $(CCOMPL_USE) -FI Precomp.h
!ENDIF

all: $(PROGPATH)

clean:
	-del /Q $(PROGPATH) $O\*.exe $O\*.dll $O\*.obj $O\*.lib $O\*.exp $O\*.res $O\*.pch $O\*.asm

$O:
	if not exist "$O" mkdir "$O"
$O/asm:
	if not exist "$O/asm" mkdir "$O/asm"

$(PROGPATH): $O $O/asm $(OBJS) $(DEF_FILE)
	link $(LFLAGS) -out:$(PROGPATH) $(OBJS) $(LIBS)

!IFNDEF NO_DEFAULT_RES
$O\resource.res: $(*B).rc
	rc $(RFLAGS) -fo$@ $**
!ENDIF
$O\StdAfx.obj: $(*B).cpp
	$(COMPL_PCH)
