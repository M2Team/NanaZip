COMMON_OBJS = $(COMMON_OBJS) \
  $O\Sha1Prepare.obj

C_OBJS = $(C_OBJS) \
  $O\Sha1.obj

!IF defined(USE_C_SHA) || "$(PLATFORM)" == "arm" || "$(PLATFORM)" == "arm64"
C_OBJS = $(C_OBJS) \
  $O\Sha1Opt.obj
!ELSEIF "$(PLATFORM)" != "ia64" && "$(PLATFORM)" != "mips" && "$(PLATFORM)" != "arm" && "$(PLATFORM)" != "arm64"
ASM_OBJS = $(ASM_OBJS) \
  $O\Sha1Opt.obj
!ENDIF
