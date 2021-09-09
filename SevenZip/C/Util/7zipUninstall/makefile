PROG = 7zipUninstall.exe
MY_FIXED = 1

!IFDEF _64BIT_INSTALLER
CFLAGS = $(CFLAGS) -D_64BIT_INSTALLER
!ENDIF

MAIN_OBJS = \
  $O\7zipUninstall.obj \

OBJS = \
  $(MAIN_OBJS) \
  $O\resource.res

!include "../../../CPP/Build.mak"

$(MAIN_OBJS): $(*B).c
	$(COMPL_O1)
