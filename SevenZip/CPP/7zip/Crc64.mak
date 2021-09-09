C_OBJS = $(C_OBJS) \
  $O\XzCrc64.obj
!IF "$(PLATFORM)" == "ia64" || "$(PLATFORM)" == "mips" || "$(PLATFORM)" == "arm" || "$(PLATFORM)" == "arm64"
C_OBJS = $(C_OBJS) \
!ELSE
ASM_OBJS = $(ASM_OBJS) \
!ENDIF
  $O\XzCrc64Opt.obj
