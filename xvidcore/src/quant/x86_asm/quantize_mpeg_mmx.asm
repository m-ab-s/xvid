;/**************************************************************************
; *
; *  XVID MPEG-4 VIDEO CODEC
; *  - 3dne Quantization/Dequantization -
; *
; *  Copyright (C) 2002-2003 Peter Ross <pross@xvid.org>
; *                2002-2003 Michael Militzer <isibaar@xvid.org>
; *                2002-2003 Pascal Massimino <skal@planet-d.net>
; *
; *  This program is free software ; you can redistribute it and/or modify
; *  it under the terms of the GNU General Public License as published by
; *  the Free Software Foundation ; either version 2 of the License, or
; *  (at your option) any later version.
; *
; *  This program is distributed in the hope that it will be useful,
; *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
; *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; *  GNU General Public License for more details.
; *
; *  You should have received a copy of the GNU General Public License
; *  along with this program ; if not, write to the Free Software
; *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
; *
; * $Id: quantize_mpeg_mmx.asm,v 1.8 2006-09-22 03:40:11 syskin Exp $
; *
; *************************************************************************/

%define SATURATE

BITS 32

%macro cglobal 1
	%ifdef PREFIX
		%ifdef MARK_FUNCS
			global _%1:function %1.endfunc-%1
			%define %1 _%1:function %1.endfunc-%1
		%else
			global _%1
			%define %1 _%1
		%endif
	%else
		%ifdef MARK_FUNCS
			global %1:function %1.endfunc-%1
		%else
			global %1
		%endif
	%endif
%endmacro

%macro cextern 1
	%ifdef PREFIX
		extern _%1
		%define %1 _%1
	%else
		extern %1
	%endif
%endmacro

;=============================================================================
; Local data (Read Only)
;=============================================================================

%ifdef FORMAT_COFF
SECTION .rodata
%else
SECTION .rodata align=16
%endif

mmx_one:
	times 4	dw	 1

;-----------------------------------------------------------------------------
; divide by 2Q table
;-----------------------------------------------------------------------------

ALIGN 16
mmx_div:
	times 4 dw 65535 ; the div by 2 formula will overflow for the case
	                 ; quant=1 but we don't care much because quant=1
	                 ; is handled by a different piece of code that
	                 ; doesn't use this table.
%assign quant 2
%rep 30
	times 4 dw  (1<<17) / (quant*2) + 1
	%assign quant quant+1
%endrep

%define VM18P 3
%define VM18Q 4


;-----------------------------------------------------------------------------
; quantd table
;-----------------------------------------------------------------------------

quantd:
%assign quant 1
%rep 31
	times 4 dw  ((VM18P*quant) + (VM18Q/2)) / VM18Q
	%assign quant quant+1
%endrep

;-----------------------------------------------------------------------------
; multiple by 2Q table
;-----------------------------------------------------------------------------

mmx_mul_quant:
%assign quant 1
%rep 31
	times 4 dw  quant
	%assign quant quant+1
%endrep

;-----------------------------------------------------------------------------
; saturation limits
;-----------------------------------------------------------------------------

ALIGN 16

mmx_32767_minus_2047:
	times 4 dw (32767-2047)
mmx_32768_minus_2048:
	times 4 dw (32768-2048)
mmx_2047:
	times 4 dw 2047
mmx_minus_2048:
	times 4 dw (-2048)
zero:
	times 4 dw 0

;=============================================================================
; rounding
;=============================================================================

mmx_rounding:
	dw (1<<13)
	dw 0
	dw (1<<13)
	dw 0

;=============================================================================
; Code
;=============================================================================

SECTION .text

cglobal quant_mpeg_intra_mmx
cglobal quant_mpeg_inter_mmx
cglobal dequant_mpeg_intra_mmx
cglobal dequant_mpeg_inter_mmx


%macro QUANT_MMX	1
	movq	mm0, [eax + 16*(%1)]			; data
	movq	mm2, [ecx + 16*(%1) + 128]		; intra_matrix_rec
	movq	mm4, [eax + 16*(%1) + 8]		; data
	movq	mm6, [ecx + 16*(%1) + 128 + 8]	; intra_matrix_rec
	
	movq	mm1, mm0
	movq	mm5, mm4

	pmullw	mm0, mm2					; low results
	pmulhw	mm1, mm2					; high results
	pmullw	mm4, mm6					; low results
	pmulhw	mm5, mm6					; high results

	movq	mm2, mm0
	movq	mm6, mm4

	punpckhwd mm0, mm1
	punpcklwd mm2, mm1
	punpckhwd mm4, mm5
	punpcklwd mm6, mm5

	paddd	mm2, mm7
	paddd	mm0, mm7
	paddd	mm6, mm7
	paddd	mm4, mm7

	psrad	mm2, 14
	psrad	mm0, 14
	psrad	mm6, 14
	psrad	mm4, 14
	
	packssdw mm2, mm0
	packssdw mm6, mm4

	movq	[edi + 16*(%1)], mm2
	movq	[edi + 16*(%1)+8], mm6
%endmacro

;-----------------------------------------------------------------------------
;
; uint32_t quant_mpeg_intra_mmx(int16_t * coeff,
;                               const int16_t const * data,
;                               const uint32_t quant,
;                               const uint32_t dcscalar,
;                               const uint16_t *mpeg_matrices);
;
;-----------------------------------------------------------------------------

ALIGN 16
quant_mpeg_intra_mmx:

  push edi
  movq mm7, [mmx_rounding]

  mov eax, [esp + 4 + 8]		; data
  mov ecx, [esp + 4 + 20]		; mpeg_quant_matrices
  mov edi, [esp + 4 + 4]		; coeff

  QUANT_MMX(0)
  QUANT_MMX(1)
  QUANT_MMX(2)
  QUANT_MMX(3)
  QUANT_MMX(4)
  QUANT_MMX(5)
  QUANT_MMX(6)
  QUANT_MMX(7)

  ; calculate DC
  movsx eax, word [eax]     ; data[0]
  mov ecx, [esp + 4 + 16]   ; dcscalar
  mov edx, eax
  mov edi, ecx
  shr ecx, 1                ; ecx = dcscalar/2
  sar edx, 31               ; edx = sign extend of eax (ready for division too)
  xor ecx, edx              ; adjust ecx according to the sign of data[0]
  sub ecx, edx
  add eax, ecx

  mov ecx, [esp + 4 + 4]	; coeff again 
  idiv edi                  ; eax = edx:eax / dcscalar
  mov [ecx], ax             ; coeff[0] = ax
 
  pop edi

  xor eax, eax              ; return(0);
  ret
.endfunc


;-----------------------------------------------------------------------------
;
; uint32_t quant_mpeg_inter_mmx(int16_t * coeff,
;                               const int16_t const * data,
;                               const uint32_t quant,
;                               const uint16_t *mpeg_matrices);
;
;-----------------------------------------------------------------------------

ALIGN 16
quant_mpeg_inter_mmx:

  push ecx
  push esi
  push edi
  push ebx

  mov edi, [esp + 16 + 4]       ; coeff
  mov esi, [esp + 16 + 8]       ; data
  mov eax, [esp + 16 + 12]  ; quant
  mov ebx, [esp + 16 + 16]		; mpeg_quant_matrices

  xor ecx, ecx

  pxor mm5, mm5                 ; sum

  cmp al, 1
  jz near .q1loop

  cmp al, 2
  jz near .q2loop

  movq mm7, [mmx_div + eax * 8 - 8] ; divider

ALIGN 16
.loop
  movq mm0, [esi + 8*ecx]       ; mm0 = [1st]
  movq mm3, [esi + 8*ecx + 8]   ;
  pxor mm1, mm1                 ; mm1 = 0
  pxor mm4, mm4                 ;
  pcmpgtw mm1, mm0              ; mm1 = (0 > mm0)
  pcmpgtw mm4, mm3              ;
  pxor mm0, mm1                 ; mm0 = |mm0|
  pxor mm3, mm4                 ;
  psubw mm0, mm1                ; displace
  psubw mm3, mm4                ;
  psllw mm0, 4
  psllw mm3, 4
  movq mm2, [ebx + 512 + 8*ecx]
  psrlw mm2, 1
  paddw mm0, mm2
  movq mm2, [ebx + 768 + ecx*8]
  pmulhw mm0, mm2               ; (level<<4 + inter_matrix[i]>>1) / inter_matrix[i]
  movq mm2, [ebx + 512 + 8*ecx + 8]
  psrlw mm2, 1
  paddw mm3, mm2
  movq mm2, [ebx + 768 + ecx*8 + 8]
  pmulhw mm3, mm2
  pmulhw mm0, mm7               ; mm0 = (mm0 / 2Q) >> 16
  pmulhw mm3, mm7               ;
  psrlw mm0, 1                  ; additional shift by 1 => 16 + 1 = 17
  psrlw mm3, 1
  paddw mm5, mm0                ; sum += mm0
  pxor mm0, mm1                 ; mm0 *= sign(mm0)
  paddw mm5, mm3                ;
  pxor mm3, mm4                 ;
  psubw mm0, mm1                ; undisplace
  psubw mm3, mm4
  movq [edi + 8*ecx], mm0
  movq [edi + 8*ecx + 8], mm3

  add ecx, 2
  cmp ecx, 16
  jnz near .loop

.done
  pmaddwd mm5, [mmx_one]
  movq mm0, mm5
  psrlq mm5, 32
  paddd mm0, mm5
  movd eax, mm0                 ; return sum

  pop ebx
  pop edi
  pop esi
  pop ecx

  ret

ALIGN 16
.q1loop
  movq mm0, [esi + 8*ecx]       ; mm0 = [1st]
  movq mm3, [esi + 8*ecx+ 8]
  pxor mm1, mm1                 ; mm1 = 0
  pxor mm4, mm4                 ;
  pcmpgtw mm1, mm0              ; mm1 = (0 > mm0)
  pcmpgtw mm4, mm3              ;
  pxor mm0, mm1                 ; mm0 = |mm0|
  pxor mm3, mm4                 ;
  psubw mm0, mm1                ; displace
  psubw mm3, mm4                ;
  psllw mm0, 4
  psllw mm3, 4
  movq mm2, [ebx + 512 + 8*ecx]
  psrlw mm2, 1
  paddw mm0, mm2
  movq mm2, [ebx + 768 + ecx*8]
  pmulhw mm0, mm2               ; (level<<4 + inter_matrix[i]>>1) / inter_matrix[i]
  movq mm2, [ebx + 512 + 8*ecx + 8]
  psrlw mm2, 1
  paddw mm3, mm2
  movq mm2, [ebx + 768 + ecx*8 + 8]
  pmulhw mm3, mm2
  psrlw mm0, 1                  ; mm0 >>= 1   (/2)
  psrlw mm3, 1                  ;
  paddw mm5, mm0                ; sum += mm0
  pxor mm0, mm1                 ; mm0 *= sign(mm0)
  paddw mm5, mm3                ;
  pxor mm3, mm4                 ;
  psubw mm0, mm1                ; undisplace
  psubw mm3, mm4
  movq [edi + 8*ecx], mm0
  movq [edi + 8*ecx + 8], mm3

  add ecx, 2
  cmp ecx, 16
  jnz near .q1loop

  jmp .done

ALIGN 16
.q2loop
  movq mm0, [esi + 8*ecx]       ; mm0 = [1st]
  movq mm3, [esi + 8*ecx+ 8]
  pxor mm1, mm1                 ; mm1 = 0
  pxor mm4, mm4                 ;
  pcmpgtw mm1, mm0              ; mm1 = (0 > mm0)
  pcmpgtw mm4, mm3              ;
  pxor mm0, mm1                 ; mm0 = |mm0|
  pxor mm3, mm4                 ;
  psubw mm0, mm1                ; displace
  psubw mm3, mm4                ;
  psllw mm0, 4
  psllw mm3, 4
  movq mm2, [ebx + 512 + 8*ecx]
  psrlw mm2, 1
  paddw mm0, mm2
  movq mm2, [ebx + 768 + ecx*8]
  pmulhw mm0, mm2               ; (level<<4 + inter_matrix[i]>>1) / inter_matrix[i]
  movq mm2, [ebx + 512 + 8*ecx + 8]
  psrlw mm2, 1
  paddw mm3, mm2
  movq mm2, [ebx + 768 + ecx*8 + 8]
  pmulhw mm3, mm2
  psrlw mm0, 2                  ; mm0 >>= 1   (/2)
  psrlw mm3, 2                  ;
  paddw mm5, mm0                ; sum += mm0
  pxor mm0, mm1                 ; mm0 *= sign(mm0)
  paddw mm5, mm3                ;
  pxor mm3, mm4                 ;
  psubw mm0, mm1                ; undisplace
  psubw mm3, mm4
  movq [edi + 8*ecx], mm0
  movq [edi + 8*ecx + 8], mm3

  add ecx, 2
  cmp ecx, 16
  jnz near .q2loop

  jmp .done
.endfunc


;-----------------------------------------------------------------------------
;
; uint32_t dequant_mpeg_intra_mmx(int16_t *data,
;                                 const int16_t const *coeff,
;                                 const uint32_t quant,
;                                 const uint32_t dcscalar,
;                                 const uint16_t *mpeg_matrices);
;
;-----------------------------------------------------------------------------

  ;   Note: in order to saturate 'easily', we pre-shift the quantifier
  ; by 4. Then, the high-word of (coeff[]*matrix[i]*quant) are used to
  ; build a saturating mask. It is non-zero only when an overflow occured.
  ; We thus avoid packing/unpacking toward double-word.
  ; Moreover, we perform the mult (matrix[i]*quant) first, instead of, e.g.,
  ; (coeff[i]*matrix[i]). This is less prone to overflow if coeff[] are not
  ; checked. Input ranges are: coeff in [-127,127], inter_matrix in [1..255],a
  ; and quant in [1..31].
  ;
  ; The original loop is:
  ;
%if 0
  movq mm0, [ecx+8*eax + 8*16]   ; mm0 = coeff[i]
  pxor mm1, mm1
  pcmpgtw mm1, mm0
  pxor mm0, mm1     ; change sign if negative
  psubw mm0, mm1    ; -> mm0 = abs(coeff[i]), mm1 = sign of coeff[i]

  movq mm2, mm7     ; mm2 = quant
  pmullw mm2, [ebx + 8*eax + 8*16 ]  ; matrix[i]*quant.

  movq mm6, mm2
  pmulhw mm2, mm0   ; high of coeff*(matrix*quant)  (should be 0 if no overflow)
  pmullw mm0, mm6   ; low  of coeff*(matrix*quant)

  pxor mm5, mm5
  pcmpgtw mm2, mm5  ; otherflow?
  psrlw mm2, 5      ; =0 if no clamp, 2047 otherwise
  psrlw mm0, 5
  paddw mm0, mm1    ; start restoring sign
  por mm0, mm2      ; saturate to 2047 if needed
  pxor mm0, mm1     ; finish negating back

  movq [edx + 8*eax + 8*16], mm0   ; data[i]
  add eax, 1
%endif

  ;********************************************************************

ALIGN 16
dequant_mpeg_intra_mmx:

  push ebx

  mov edx, [esp + 4 + 4]  ; data
  mov ecx, [esp + 4 + 8]  ; coeff
  mov eax, [esp + 4 + 12] ; quant
  mov ebx, [esp + 4 + 20] ; mpeg_quant_matrices

  movq mm7, [mmx_mul_quant  + eax*8 - 8]
  mov eax, -16      ; to keep ALIGNed, we regularly process coeff[0]
  psllw mm7, 2      ; << 2. See comment.
  pxor mm6, mm6     ; this is a NOP

ALIGN 16
.loop
  movq mm0, [ecx+8*eax + 8*16]   ; mm0 = c  = coeff[i]
  movq mm3, [ecx+8*eax + 8*16 +8]; mm3 = c' = coeff[i+1]
  pxor mm1, mm1
  pxor mm4, mm4
  pcmpgtw mm1, mm0  ; mm1 = sgn(c)
  movq mm2, mm7     ; mm2 = quant

  pcmpgtw mm4, mm3  ; mm4 = sgn(c')
  pmullw mm2,  [ebx + 8*eax + 8*16 ]  ; matrix[i]*quant

  pxor mm0, mm1     ; negate if negative
  pxor mm3, mm4     ; negate if negative

  psubw mm0, mm1
  psubw mm3, mm4

 ; we're short on register, here. Poor pairing...

  movq mm5, mm2
  pmullw mm2, mm0   ; low  of coeff*(matrix*quant)

  pmulhw mm0, mm5   ; high of coeff*(matrix*quant)
  movq mm5, mm7     ; mm2 = quant

  pmullw mm5,  [ebx + 8*eax + 8*16 +8]  ; matrix[i+1]*quant

  movq mm6, mm5
  add eax,2   ; z-flag will be tested later

  pmullw mm6, mm3   ; low  of coeff*(matrix*quant)
  pmulhw mm3, mm5   ; high of coeff*(matrix*quant)

  pcmpgtw mm0, [zero]
  paddusw mm2, mm0
  psrlw mm2, 5

  pcmpgtw mm3, [zero]
  paddusw mm6, mm3
  psrlw mm6, 5

  pxor mm2, mm1  ; start negating back
  pxor mm6, mm4  ; start negating back

  psubusw mm1, mm0
  psubusw mm4, mm3

  psubw mm2, mm1 ; finish negating back
  psubw mm6, mm4 ; finish negating back

  movq [edx + 8*eax + 8*16   -2*8   ], mm2   ; data[i]
  movq [edx + 8*eax + 8*16   -2*8 +8], mm6   ; data[i+1]

 jnz        near .loop

    ; deal with DC
  movd mm0, [ecx]
  pmullw mm0, [esp + 4 + 16]  ; dcscalar
  movq mm2, [mmx_32767_minus_2047]
  paddsw mm0, mm2
  psubsw mm0, mm2
  movq mm2, [mmx_32768_minus_2048]
  psubsw mm0, mm2
  paddsw mm0, mm2
  movd eax, mm0
  mov [edx], ax

  xor eax, eax
  
  pop ebx

  ret
.endfunc

;-----------------------------------------------------------------------------
;
; uint32_t dequant_mpeg_inter_mmx(int16_t * data,
;                                 const int16_t * const coeff,
;                                 const uint32_t quant,
;                                 const uint16_t *mpeg_matrices);
;
;-----------------------------------------------------------------------------

    ; Note:  We use (2*c + sgn(c) - sgn(-c)) as multiplier
    ; so we handle the 3 cases: c<0, c==0, and c>0 in one shot.
    ; sgn(x) is the result of 'pcmpgtw 0,x':  0 if x>=0, -1 if x<0.
    ; It's mixed with the extraction of the absolute value.

ALIGN 16
dequant_mpeg_inter_mmx:

  push ebx

  mov edx, [esp + 4 + 4]        ; data
  mov ecx, [esp + 4 + 8]        ; coeff
  mov eax, [esp + 4 + 12]        ; quant
  mov ebx, [esp + 4 + 16]		   ; mpeg_quant_matrices

  movq mm7, [mmx_mul_quant  + eax*8 - 8]
  mov eax, -16
  paddw mm7, mm7    ; << 1
  pxor mm6, mm6     ; mismatch sum

ALIGN 16
.loop
  movq mm0, [ecx+8*eax + 8*16   ]   ; mm0 = coeff[i]
  movq mm2, [ecx+8*eax + 8*16 +8]   ; mm2 = coeff[i+1]
  add eax, 2

  pxor mm1, mm1
  pxor mm3, mm3
  pcmpgtw mm1, mm0  ; mm1 = sgn(c)    (preserved)
  pcmpgtw mm3, mm2  ; mm3 = sgn(c')   (preserved)
  paddsw mm0, mm1   ; c += sgn(c)
  paddsw mm2, mm3   ; c += sgn(c')
  paddw mm0, mm0    ; c *= 2
  paddw mm2, mm2    ; c'*= 2

  pxor mm4, mm4
  pxor mm5, mm5
  psubw mm4, mm0    ; -c
  psubw mm5, mm2    ; -c'
  psraw mm4, 16     ; mm4 = sgn(-c)
  psraw mm5, 16     ; mm5 = sgn(-c')
  psubsw mm0, mm4   ; c  -= sgn(-c)
  psubsw mm2, mm5   ; c' -= sgn(-c')
  pxor mm0, mm1     ; finish changing sign if needed
  pxor mm2, mm3     ; finish changing sign if needed

    ; we're short on register, here. Poor pairing...

  movq mm4, mm7     ; (matrix*quant)
  pmullw mm4,  [ebx + 512 + 8*eax + 8*16 -2*8]
  movq mm5, mm4
  pmulhw mm5, mm0   ; high of c*(matrix*quant)
  pmullw mm0, mm4   ; low  of c*(matrix*quant)

  movq mm4, mm7     ; (matrix*quant)
  pmullw mm4,  [ebx + 512 + 8*eax + 8*16 -2*8 + 8]

  pcmpgtw mm5, [zero]
  paddusw mm0, mm5
  psrlw mm0, 5
  pxor mm0, mm1     ; start restoring sign
  psubusw mm1, mm5

  movq mm5, mm4
  pmulhw mm5, mm2   ; high of c*(matrix*quant)
  pmullw mm2, mm4   ; low  of c*(matrix*quant)
  psubw mm0, mm1    ; finish restoring sign

  pcmpgtw mm5, [zero]
  paddusw mm2, mm5
  psrlw mm2, 5
  pxor mm2, mm3     ; start restoring sign
  psubusw mm3, mm5
  psubw mm2, mm3    ; finish restoring sign

  pxor mm6, mm0     ; mismatch control
  movq [edx + 8*eax + 8*16 -2*8   ], mm0   ; data[i]
  pxor mm6, mm2     ; mismatch control
  movq [edx + 8*eax + 8*16 -2*8 +8], mm2   ; data[i+1]

  jnz near .loop

  ; mismatch control

  movq mm0, mm6
  psrlq mm0, 48
  movq mm1, mm6
  movq mm2, mm6
  psrlq mm1, 32
  pxor mm6, mm0
  psrlq mm2, 16
  pxor mm6, mm1
  pxor mm6, mm2
  movd eax, mm6
  and eax, 1
  xor eax, 1
  xor word [edx + 2*63], ax

  xor eax, eax
  
  pop ebx

  ret
.endfunc

