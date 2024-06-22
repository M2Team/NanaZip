; Sha1Opt.asm -- SHA-1 optimized code for SHA-1 x86 hardware instructions
; 2024-06-16 : Igor Pavlov : Public domain

include 7zAsm.asm

MY_ASM_START
















CONST   SEGMENT READONLY

align 16
Reverse_Endian_Mask db 15,14,13,12, 11,10,9,8, 7,6,5,4, 3,2,1,0






















CONST   ENDS

; _TEXT$SHA1OPT SEGMENT 'CODE'

ifndef x64
    .686
    .xmm
endif

ifdef x64
        rNum    equ REG_ABI_PARAM_2
    if (IS_LINUX eq 0)
        LOCAL_SIZE equ (16 * 2)
    endif
else
        rNum    equ r0
        LOCAL_SIZE equ (16 * 1)
endif

rState equ REG_ABI_PARAM_0
rData  equ REG_ABI_PARAM_1


MY_sha1rnds4 macro a1, a2, imm
        db 0fH, 03aH, 0ccH, (0c0H + a1 * 8 + a2), imm
endm

MY_SHA_INSTR macro cmd, a1, a2
        db 0fH, 038H, cmd, (0c0H + a1 * 8 + a2)
endm

cmd_sha1nexte   equ 0c8H
cmd_sha1msg1    equ 0c9H
cmd_sha1msg2    equ 0caH

MY_sha1nexte macro a1, a2
        MY_SHA_INSTR  cmd_sha1nexte, a1, a2
endm

MY_sha1msg1 macro a1, a2
        MY_SHA_INSTR  cmd_sha1msg1, a1, a2
endm

MY_sha1msg2 macro a1, a2
        MY_SHA_INSTR  cmd_sha1msg2, a1, a2
endm

MY_PROLOG macro
    ifdef x64
      if (IS_LINUX eq 0)
        movdqa  [r4 + 8], xmm6
        movdqa  [r4 + 8 + 16], xmm7
        sub     r4, LOCAL_SIZE + 8
        movdqa  [r4     ], xmm8
        movdqa  [r4 + 16], xmm9
      endif
    else ; x86
      if (IS_CDECL gt 0)
        mov     rState, [r4 + REG_SIZE * 1]
        mov     rData,  [r4 + REG_SIZE * 2]
        mov     rNum,   [r4 + REG_SIZE * 3]
      else ; fastcall
        mov     rNum,   [r4 + REG_SIZE * 1]
      endif
        push    r5
        mov     r5, r4
        and     r4, -16
        sub     r4, LOCAL_SIZE
    endif
endm

MY_EPILOG macro
    ifdef x64
      if (IS_LINUX eq 0)
        movdqa  xmm8, [r4]
        movdqa  xmm9, [r4 + 16]
        add     r4, LOCAL_SIZE + 8
        movdqa  xmm6, [r4 + 8]
        movdqa  xmm7, [r4 + 8 + 16]
      endif
    else ; x86
        mov     r4, r5
        pop     r5
    endif
    MY_ENDP
endm


e0_N       equ 0
e1_N       equ 1
abcd_N     equ 2
e0_save_N  equ 3
w_regs     equ 4

e0      equ @CatStr(xmm, %e0_N)
e1      equ @CatStr(xmm, %e1_N)
abcd    equ @CatStr(xmm, %abcd_N)
e0_save equ @CatStr(xmm, %e0_save_N)


ifdef x64
        abcd_save    equ  xmm8
        mask2        equ  xmm9
else
        abcd_save    equ  [r4]
        mask2        equ  e1
endif

LOAD_MASK macro
        movdqa  mask2, XMMWORD PTR Reverse_Endian_Mask
endm

LOAD_W macro k:req
        movdqu  @CatStr(xmm, %(w_regs + k)), [rData + (16 * (k))]
        pshufb  @CatStr(xmm, %(w_regs + k)), mask2
endm


; pre2 can be 2 or 3 (recommended)
pre2 equ 3
pre1 equ (pre2 + 1)

NUM_ROUNDS4 equ 20
   
RND4 macro k
        movdqa  @CatStr(xmm, %(e0_N + ((k + 1) mod 2))), abcd
        MY_sha1rnds4 abcd_N, (e0_N + (k mod 2)), k / 5

        nextM = (w_regs + ((k + 1) mod 4))

    if (k EQ NUM_ROUNDS4 - 1)
        nextM = e0_save_N
    endif
        
        MY_sha1nexte (e0_N + ((k + 1) mod 2)), nextM
        
    if (k GE (4 - pre2)) AND (k LT (NUM_ROUNDS4 - pre2))
        pxor @CatStr(xmm, %(w_regs + ((k + pre2) mod 4))), @CatStr(xmm, %(w_regs + ((k + pre2 - 2) mod 4)))
    endif

    if (k GE (4 - pre1)) AND (k LT (NUM_ROUNDS4 - pre1))
        MY_sha1msg1 (w_regs + ((k + pre1) mod 4)), (w_regs + ((k + pre1 - 3) mod 4))
    endif
    
    if (k GE (4 - pre2)) AND (k LT (NUM_ROUNDS4 - pre2))
        MY_sha1msg2 (w_regs + ((k + pre2) mod 4)), (w_regs + ((k + pre2 - 1) mod 4))
    endif
endm


REVERSE_STATE macro
                               ; abcd   ; dcba
                               ; e0     ; 000e
        pshufd  abcd, abcd, 01bH        ; abcd
        pshufd    e0,   e0, 01bH        ; e000
endm





MY_PROC Sha1_UpdateBlocks_HW, 3
    MY_PROLOG

        cmp     rNum, 0
        je      end_c

        movdqu   abcd, [rState]               ; dcba
        movd     e0, dword ptr [rState + 16]  ; 000e

        REVERSE_STATE
       
        ifdef x64
        LOAD_MASK
        endif

    align 16
    nextBlock:
        movdqa  abcd_save, abcd
        movdqa  e0_save, e0
        
        ifndef x64
        LOAD_MASK
        endif
        
        LOAD_W 0
        LOAD_W 1
        LOAD_W 2
        LOAD_W 3

        paddd   e0, @CatStr(xmm, %(w_regs))
        k = 0
        rept NUM_ROUNDS4
          RND4 k
          k = k + 1
        endm

        paddd   abcd, abcd_save


        add     rData, 64
        sub     rNum, 1
        jnz     nextBlock
        
        REVERSE_STATE

        movdqu  [rState], abcd
        movd    dword ptr [rState + 16], e0
       
  end_c:
MY_EPILOG

; _TEXT$SHA1OPT ENDS

end
