;/*****************************************************************************
; *
; *  XVID MPEG-4 VIDEO CODEC
; *  mmx optimized MPEG quantization/dequantization             
; *
; *  Copyright(C) 2002 Peter Ross <pross@xvid.org>
; *  Copyright(C) 2002 Michael Militzer <michael@xvid.org>
; *  Copyright(C) 2002 Pascal Massimino <skal@planet-d.net>
; *
; *  This file is part of XviD, a free MPEG-4 video encoder/decoder
; *
; *  XviD is free software; you can redistribute it and/or modify it
; *  under the terms of the GNU General Public License as published by
; *  the Free Software Foundation; either version 2 of the License, or
; *  (at your option) any later version.
; *
; *  This program is distributed in the hope that it will be useful,
; *  but WITHOUT ANY WARRANTY; without even the implied warranty of
; *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; *  GNU General Public License for more details.
; *
; *  You should have received a copy of the GNU General Public License
; *  along with this program; if not, write to the Free Software
; *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
; *
; *  Under section 8 of the GNU General Public License, the copyright
; *  holders of XVID explicitly forbid distribution in the following
; *  countries:
; *
; *    - Japan
; *    - United States of America
; *
; *  Linking XviD statically or dynamically with other modules is making a
; *  combined work based on XviD.  Thus, the terms and conditions of the
; *  GNU General Public License cover the whole combination.
; *
; *  As a special exception, the copyright holders of XviD give you
; *  permission to link XviD with independent modules that communicate with
; *  XviD solely through the VFW1.1 and DShow interfaces, regardless of the
; *  license terms of these independent modules, and to copy and distribute
; *  the resulting combined work under terms of your choice, provided that
; *  every copy of the combined work is accompanied by a complete copy of
; *  the source code of XviD (the version of XviD used to produce the
; *  combined work), being distributed under the terms of the GNU General
; *  Public License plus this exception.  An independent module is a module
; *  which is not derived from or based on XviD.
; *
; *  Note that people who make modified versions of XviD are not obligated
; *  to grant this special exception for their modified versions; it is
; *  their choice whether to do so.  The GNU General Public License gives
; *  permission to release a modified version without this exception; this
; *  exception also makes it possible to release a modified version which
; *  carries forward this exception.
; *
; * $Id: quantize4_mmx.asm,v 1.7 2002-11-17 00:41:20 edgomez Exp $
; *
; *************************************************************************/

; data/text alignment
%define ALIGN 8

%define SATURATE

bits 32

section .data

%macro cglobal 1 
	%ifdef PREFIX
		global _%1 
		%define %1 _%1
	%else
		global %1
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

mmx_one times 4	dw	 1

;===========================================================================
;
; divide by 2Q table 
;
;===========================================================================

%macro MMX_DIV  1
times 4 dw  (1 << 17) / (%1 * 2) + 1
%endmacro

align ALIGN
mmx_div
		MMX_DIV 1
		MMX_DIV 2
		MMX_DIV 3
		MMX_DIV 4
		MMX_DIV 5
		MMX_DIV 6
		MMX_DIV 7
		MMX_DIV 8
		MMX_DIV 9
		MMX_DIV 10
		MMX_DIV 11
		MMX_DIV 12
		MMX_DIV 13
		MMX_DIV 14
		MMX_DIV 15
		MMX_DIV 16
		MMX_DIV 17
		MMX_DIV 18
		MMX_DIV 19
		MMX_DIV 20
		MMX_DIV 21
		MMX_DIV 22
		MMX_DIV 23
		MMX_DIV 24
		MMX_DIV 25
		MMX_DIV 26
		MMX_DIV 27
		MMX_DIV 28
		MMX_DIV 29
		MMX_DIV 30
		MMX_DIV 31


;===========================================================================
;
; intra matrix 
;
;===========================================================================

cextern intra_matrix
cextern intra_matrix_fix

;===========================================================================
;
; inter matrix
;
;===========================================================================

cextern inter_matrix
cextern inter_matrix_fix


%define VM18P 3
%define VM18Q 4


;===========================================================================
;
; quantd table 
;
;===========================================================================

%macro MMX_QUANTD  1
times 4 dw ((VM18P*%1) + (VM18Q/2)) / VM18Q
%endmacro

quantd
		MMX_QUANTD 1 
		MMX_QUANTD 2 
		MMX_QUANTD 3 
		MMX_QUANTD 4 
		MMX_QUANTD 5 
		MMX_QUANTD 6 
		MMX_QUANTD 7 
		MMX_QUANTD 8 
		MMX_QUANTD 9 
		MMX_QUANTD 10 
		MMX_QUANTD 11
		MMX_QUANTD 12
		MMX_QUANTD 13
		MMX_QUANTD 14
		MMX_QUANTD 15
		MMX_QUANTD 16
		MMX_QUANTD 17
		MMX_QUANTD 18
		MMX_QUANTD 19
		MMX_QUANTD 20
		MMX_QUANTD 21
		MMX_QUANTD 22
		MMX_QUANTD 23
		MMX_QUANTD 24
		MMX_QUANTD 25
		MMX_QUANTD 26
		MMX_QUANTD 27
		MMX_QUANTD 28
		MMX_QUANTD 29
		MMX_QUANTD 30
		MMX_QUANTD 31


;===========================================================================
;
; multiple by 2Q table
;
;===========================================================================

%macro MMX_MUL_QUANT  1
times 4   dw  %1
%endmacro

mmx_mul_quant
	MMX_MUL_QUANT 1
	MMX_MUL_QUANT 2
	MMX_MUL_QUANT 3
	MMX_MUL_QUANT 4
	MMX_MUL_QUANT 5
	MMX_MUL_QUANT 6
	MMX_MUL_QUANT 7
	MMX_MUL_QUANT 8
	MMX_MUL_QUANT 9
	MMX_MUL_QUANT 10
	MMX_MUL_QUANT 11
	MMX_MUL_QUANT 12
	MMX_MUL_QUANT 13
	MMX_MUL_QUANT 14
	MMX_MUL_QUANT 15
	MMX_MUL_QUANT 16
	MMX_MUL_QUANT 17
	MMX_MUL_QUANT 18
	MMX_MUL_QUANT 19
	MMX_MUL_QUANT 20
	MMX_MUL_QUANT 21
	MMX_MUL_QUANT 22
	MMX_MUL_QUANT 23
	MMX_MUL_QUANT 24
	MMX_MUL_QUANT 25
	MMX_MUL_QUANT 26
	MMX_MUL_QUANT 27
	MMX_MUL_QUANT 28
	MMX_MUL_QUANT 29
	MMX_MUL_QUANT 30
	MMX_MUL_QUANT 31

;===========================================================================
;
; saturation limits 
;
;===========================================================================

align 16

mmx_32767_minus_2047				times 4 dw (32767-2047)
mmx_32768_minus_2048				times 4 dw (32768-2048)
mmx_2047 times 4 dw 2047
mmx_minus_2048 times 4 dw (-2048)
zero times 4 dw 0

section .text

;===========================================================================
;
; void quant_intra4_mmx(int16_t * coeff, 
;					const int16_t const * data,
;					const uint32_t quant,
;					const uint32_t dcscalar);
;
;===========================================================================

align ALIGN
cglobal quant4_intra_mmx
quant4_intra_mmx

		push	ecx
		push	esi
		push	edi

		mov	edi, [esp + 12 + 4]		; coeff
		mov	esi, [esp + 12 + 8]		; data
		mov	eax, [esp + 12 + 12]	; quant

		movq    mm5, [quantd + eax * 8 - 8] ; quantd -> mm5

		xor ecx, ecx
		cmp	al, 1
		jz	near .q1loop

		cmp	al, 2
		jz	near .q2loop

		movq	mm7, [mmx_div + eax * 8 - 8] ; multipliers[quant] -> mm7
		 
align ALIGN
.loop
		movq	mm0, [esi + 8*ecx]		; mm0 = [1st]
		movq	mm3, [esi + 8*ecx + 8]	; 
		
		pxor	mm1, mm1		; mm1 = 0
		pxor	mm4, mm4
		
		pcmpgtw	mm1, mm0		; mm1 = (0 > mm0)
		pcmpgtw	mm4, mm3
		
		pxor	mm0, mm1		; mm0 = |mm0|
		pxor	mm3, mm4		;
		psubw	mm0, mm1		; displace
		psubw	mm3, mm4		;

		psllw   mm0, 4			; level << 4
		psllw   mm3, 4			;
		
		movq    mm2, [intra_matrix + 8*ecx]
		psrlw   mm2, 1			; intra_matrix[i]>>1
		paddw   mm0, mm2		
		
		movq    mm2, [intra_matrix_fix + ecx*8]
		pmulhw  mm0, mm2		; (level<<4 + intra_matrix[i]>>1) / intra_matrix[i]

		movq    mm2, [intra_matrix + 8*ecx + 8]
		psrlw   mm2, 1
		paddw   mm3, mm2

		movq    mm2, [intra_matrix_fix + ecx*8 + 8]
		pmulhw  mm3, mm2

	    paddw   mm0, mm5		; + quantd
		paddw   mm3, mm5

		pmulhw	mm0, mm7		; mm0 = (mm0 / 2Q) >> 16
		pmulhw	mm3, mm7		;
		psrlw   mm0, 1			; additional shift by 1 => 16 + 1 = 17
		psrlw   mm3, 1
		
		pxor	mm0, mm1		; mm0 *= sign(mm0)
		pxor	mm3, mm4		;
		psubw	mm0, mm1		; undisplace
		psubw	mm3, mm4		;

		movq	[edi + 8*ecx], mm0
		movq	[edi + 8*ecx + 8], mm3
		
		add ecx,2
		cmp ecx,16
		jnz 	near .loop 

.done	
		; caclulate  data[0] // (int32_t)dcscalar)

		mov 	ecx, [esp + 12 + 16]	; dcscalar
		mov 	edx, ecx
		movsx 	eax, word [esi]	; data[0]
		shr 	edx, 1			; edx = dcscalar /2
		cmp		eax, 0
		jg		.gtzero

		sub		eax, edx
		jmp		short .mul
.gtzero
		add		eax, edx
.mul
		cdq 				; expand eax -> edx:eax
		idiv	ecx			; eax = edx:eax / dcscalar
		
		mov	[edi], ax		; coeff[0] = ax

		pop	edi
		pop	esi
		pop	ecx

		ret				

align ALIGN
.q1loop
		movq	mm0, [esi + 8*ecx]		; mm0 = [1st]
		movq	mm3, [esi + 8*ecx + 8]	; 

		pxor	mm1, mm1		; mm1 = 0
		pxor	mm4, mm4		;

		pcmpgtw	mm1, mm0		; mm1 = (0 > mm0)
		pcmpgtw	mm4, mm3		;
		 
		pxor	mm0, mm1		; mm0 = |mm0|
		pxor	mm3, mm4		; 
		psubw	mm0, mm1		; displace
		psubw	mm3, mm4		; 

		psllw   mm0, 4
		psllw   mm3, 4
		
		movq    mm2, [intra_matrix + 8*ecx]
		psrlw   mm2, 1
		paddw   mm0, mm2
		
		movq    mm2, [intra_matrix_fix + ecx*8]
		pmulhw  mm0, mm2		; (level<<4 + intra_matrix[i]>>1) / intra_matrix[i]

		movq    mm2, [intra_matrix + 8*ecx + 8]
		psrlw   mm2, 1
		paddw   mm3, mm2

		movq    mm2, [intra_matrix_fix + ecx*8 + 8]
		pmulhw  mm3, mm2

        paddw   mm0, mm5
		paddw   mm3, mm5

		psrlw	mm0, 1			; mm0 >>= 1   (/2)
		psrlw	mm3, 1			;
		
		pxor	mm0, mm1		; mm0 *= sign(mm0)
		pxor	mm3, mm4        ;
		psubw	mm0, mm1		; undisplace
		psubw	mm3, mm4		;
		
		movq	[edi + 8*ecx], mm0
		movq	[edi + 8*ecx + 8], mm3

		add ecx,2
		cmp ecx,16
		jnz	near .q1loop
		jmp	near .done


align ALIGN
.q2loop
		movq	mm0, [esi + 8*ecx]		; mm0 = [1st]
		movq	mm3, [esi + 8*ecx + 8]	; 

		pxor	mm1, mm1		; mm1 = 0
		pxor	mm4, mm4		;

		pcmpgtw	mm1, mm0		; mm1 = (0 > mm0)
		pcmpgtw	mm4, mm3		;
		 
		pxor	mm0, mm1		; mm0 = |mm0|
		pxor	mm3, mm4		; 
		psubw	mm0, mm1		; displace
		psubw	mm3, mm4		; 

		psllw   mm0, 4
		psllw   mm3, 4
		
		movq    mm2, [intra_matrix + 8*ecx]
		psrlw   mm2, 1
		paddw   mm0, mm2
		
		movq    mm2, [intra_matrix_fix + ecx*8]
		pmulhw  mm0, mm2		; (level<<4 + intra_matrix[i]>>1) / intra_matrix[i]

		movq    mm2, [intra_matrix + 8*ecx + 8]
		psrlw   mm2, 1
		paddw   mm3, mm2

		movq    mm2, [intra_matrix_fix + ecx*8 + 8]
		pmulhw  mm3, mm2

        paddw   mm0, mm5
		paddw   mm3, mm5

		psrlw	mm0, 2			; mm0 >>= 1   (/4)
		psrlw	mm3, 2			;
		
		pxor	mm0, mm1		; mm0 *= sign(mm0)
		pxor	mm3, mm4        ;
		psubw	mm0, mm1		; undisplace
		psubw	mm3, mm4		;
		
		movq	[edi + 8*ecx], mm0
		movq	[edi + 8*ecx + 8], mm3

		add ecx,2
		cmp ecx,16
		jnz	near .q2loop
		jmp	near .done


;===========================================================================
;
; uint32_t quant4_inter_mmx(int16_t * coeff,
;					const int16_t const * data,
;					const uint32_t quant);
;
;===========================================================================

align ALIGN
cglobal quant4_inter_mmx
		quant4_inter_mmx

		push	ecx
		push	esi
		push	edi

		mov	edi, [esp + 12 + 4]		; coeff
		mov	esi, [esp + 12 + 8]		; data
		mov	eax, [esp + 12 + 12]	; quant

		xor ecx, ecx

		pxor mm5, mm5					; sum

		cmp	al, 1
		jz  near .q1loop

		cmp	al, 2
		jz  near .q2loop

		movq	mm7, [mmx_div + eax * 8 - 8]	; divider

align ALIGN
.loop
		movq	mm0, [esi + 8*ecx]		; mm0 = [1st]
		movq	mm3, [esi + 8*ecx + 8]	; 
		pxor	mm1, mm1		; mm1 = 0
		pxor	mm4, mm4		;
		pcmpgtw	mm1, mm0		; mm1 = (0 > mm0)
		pcmpgtw	mm4, mm3		; 
		pxor	mm0, mm1		; mm0 = |mm0|
		pxor	mm3, mm4		; 
		psubw	mm0, mm1		; displace
		psubw	mm3, mm4		;

		psllw   mm0, 4
		psllw   mm3, 4
		
		movq    mm2, [inter_matrix + 8*ecx]
		psrlw   mm2, 1
		paddw   mm0, mm2
		
		movq    mm2, [inter_matrix_fix + ecx*8]
		pmulhw  mm0, mm2		; (level<<4 + inter_matrix[i]>>1) / inter_matrix[i]

		movq    mm2, [inter_matrix + 8*ecx + 8]
		psrlw   mm2, 1
		paddw   mm3, mm2

		movq    mm2, [inter_matrix_fix + ecx*8 + 8]
		pmulhw  mm3, mm2

		pmulhw	mm0, mm7		; mm0 = (mm0 / 2Q) >> 16
		pmulhw	mm3, mm7		; 
		psrlw   mm0, 1			; additional shift by 1 => 16 + 1 = 17
		psrlw   mm3, 1
		
		paddw	mm5, mm0		; sum += mm0
		pxor	mm0, mm1		; mm0 *= sign(mm0)
		paddw	mm5, mm3		;
		pxor	mm3, mm4		;
		psubw	mm0, mm1		; undisplace
		psubw	mm3, mm4
		movq	[edi + 8*ecx], mm0
		movq	[edi + 8*ecx + 8], mm3

		add ecx, 2	
		cmp ecx, 16
		jnz near .loop

.done
		pmaddwd mm5, [mmx_one]
		movq    mm0, mm5
		psrlq   mm5, 32
		paddd   mm0, mm5
		movd	eax, mm0		; return sum

		pop	edi
		pop	esi
		pop ecx

		ret

align ALIGN
.q1loop
		movq	mm0, [esi + 8*ecx]		; mm0 = [1st]
		movq	mm3, [esi + 8*ecx+ 8]
				; 
		pxor	mm1, mm1		; mm1 = 0
		pxor	mm4, mm4		;

		pcmpgtw	mm1, mm0		; mm1 = (0 > mm0)
		pcmpgtw	mm4, mm3		;

		pxor	mm0, mm1		; mm0 = |mm0|
		pxor	mm3, mm4		; 
		psubw	mm0, mm1		; displace
		psubw	mm3, mm4		;
		
		psllw   mm0, 4
		psllw   mm3, 4
		
		movq    mm2, [inter_matrix + 8*ecx]
		psrlw   mm2, 1
		paddw   mm0, mm2
		
		movq    mm2, [inter_matrix_fix + ecx*8]
		pmulhw  mm0, mm2		; (level<<4 + inter_matrix[i]>>1) / inter_matrix[i]

		movq    mm2, [inter_matrix + 8*ecx + 8]
		psrlw   mm2, 1
		paddw   mm3, mm2

		movq    mm2, [inter_matrix_fix + ecx*8 + 8]
		pmulhw  mm3, mm2
 
		psrlw	mm0, 1			; mm0 >>= 1   (/2)
		psrlw	mm3, 1			;

		paddw	mm5, mm0		; sum += mm0
		pxor	mm0, mm1		; mm0 *= sign(mm0)
		paddw	mm5, mm3		;
		pxor	mm3, mm4		;
		psubw	mm0, mm1		; undisplace
		psubw	mm3, mm4

		movq	[edi + 8*ecx], mm0
		movq	[edi + 8*ecx + 8], mm3
		
		add ecx,2
		cmp ecx,16
		jnz	near .q1loop

		jmp	.done


align ALIGN
.q2loop
		movq	mm0, [esi + 8*ecx]		; mm0 = [1st]
		movq	mm3, [esi + 8*ecx+ 8]
				; 
		pxor	mm1, mm1		; mm1 = 0
		pxor	mm4, mm4		;

		pcmpgtw	mm1, mm0		; mm1 = (0 > mm0)
		pcmpgtw	mm4, mm3		;

		pxor	mm0, mm1		; mm0 = |mm0|
		pxor	mm3, mm4		; 
		psubw	mm0, mm1		; displace
		psubw	mm3, mm4		;
		
		psllw   mm0, 4
		psllw   mm3, 4
		
		movq    mm2, [inter_matrix + 8*ecx]
		psrlw   mm2, 1
		paddw   mm0, mm2
		
		movq    mm2, [inter_matrix_fix + ecx*8]
		pmulhw  mm0, mm2		; (level<<4 + inter_matrix[i]>>1) / inter_matrix[i]

		movq    mm2, [inter_matrix + 8*ecx + 8]
		psrlw   mm2, 1
		paddw   mm3, mm2

		movq    mm2, [inter_matrix_fix + ecx*8 + 8]
		pmulhw  mm3, mm2
 
		psrlw	mm0, 2			; mm0 >>= 1   (/2)
		psrlw	mm3, 2			;

		paddw	mm5, mm0		; sum += mm0
		pxor	mm0, mm1		; mm0 *= sign(mm0)
		paddw	mm5, mm3		;
		pxor	mm3, mm4		;
		psubw	mm0, mm1		; undisplace
		psubw	mm3, mm4

		movq	[edi + 8*ecx], mm0
		movq	[edi + 8*ecx + 8], mm3
		
		add ecx,2
		cmp ecx,16
		jnz	near .q2loop

		jmp	.done


;===========================================================================
;
; void dequant4_intra_mmx(int16_t *data,
;                    const int16_t const *coeff,
;                    const uint32_t quant,
;                    const uint32_t dcscalar);
;
;===========================================================================

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
  pmullw mm2,  [intra_matrix + 8*eax + 8*16 ]  ; matrix[i]*quant.

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

align 16
cglobal dequant4_intra_mmx
dequant4_intra_mmx:

  mov edx, [esp+4]  ; data
  mov ecx, [esp+8]  ; coeff
  mov eax, [esp+12] ; quant

  movq mm7, [mmx_mul_quant  + eax*8 - 8]
  mov eax, -16   ; to keep aligned, we regularly process coeff[0]
  psllw mm7, 2   ; << 2. See comment.
  pxor mm6, mm6   ; this is a NOP

align 16
.loop
  movq mm0, [ecx+8*eax + 8*16]   ; mm0 = c  = coeff[i]
  movq mm3, [ecx+8*eax + 8*16 +8]; mm3 = c' = coeff[i+1]
  pxor mm1, mm1
  pxor mm4, mm4
  pcmpgtw mm1, mm0  ; mm1 = sgn(c)
  movq mm2, mm7     ; mm2 = quant
  
  pcmpgtw mm4, mm3  ; mm4 = sgn(c')
  pmullw mm2,  [intra_matrix + 8*eax + 8*16 ]  ; matrix[i]*quant

  pxor mm0, mm1     ; negate if negative
  pxor mm3, mm4     ; negate if negative
  
  psubw mm0, mm1
  psubw mm3, mm4
  
    ; we're short on register, here. Poor pairing...

  movq mm5, mm2 
  pmullw mm2, mm0   ; low  of coeff*(matrix*quant)

  pmulhw mm0, mm5   ; high of coeff*(matrix*quant)
  movq mm5, mm7     ; mm2 = quant

  pmullw mm5,  [intra_matrix + 8*eax + 8*16 +8]  ; matrix[i+1]*quant

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

  jnz near .loop

    ; deal with DC

  movd mm0, [ecx]
  pmullw mm0, [esp+16]  ; dcscalar
  movq mm2, [mmx_32767_minus_2047]
  paddsw mm0, mm2
  psubsw mm0, mm2
  movq mm2, [mmx_32768_minus_2048]
  psubsw mm0, mm2
  paddsw mm0, mm2
  movd eax, mm0
  mov [edx], ax

  ret

;===========================================================================
;
; void dequant4_inter_mmx(int16_t * data,
;                    const int16_t * const coeff,
;                    const uint32_t quant);
;
;===========================================================================

    ; Note:  We use (2*c + sgn(c) - sgn(-c)) as multiplier
    ; so we handle the 3 cases: c<0, c==0, and c>0 in one shot.
    ; sgn(x) is the result of 'pcmpgtw 0,x':  0 if x>=0, -1 if x<0.
    ; It's mixed with the extraction of the absolute value.

align 16
cglobal dequant4_inter_mmx
dequant4_inter_mmx:

  mov    edx, [esp+ 4]        ; data
  mov    ecx, [esp+ 8]        ; coeff
  mov    eax, [esp+12]        ; quant
  movq mm7, [mmx_mul_quant  + eax*8 - 8]
  mov eax, -16
  paddw mm7, mm7    ; << 1
  pxor mm6, mm6 ; mismatch sum

align 16
.loop
  movq mm0, [ecx+8*eax + 8*16   ]   ; mm0 = coeff[i]
  movq mm2, [ecx+8*eax + 8*16 +8]   ; mm2 = coeff[i+1]
  add eax,2

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
  pmullw mm4,  [inter_matrix + 8*eax + 8*16 -2*8]
  movq mm5, mm4
  pmulhw mm5, mm0   ; high of c*(matrix*quant)
  pmullw mm0, mm4   ; low  of c*(matrix*quant)

  movq mm4, mm7     ; (matrix*quant)
  pmullw mm4,  [inter_matrix + 8*eax + 8*16 -2*8 + 8]

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
  pxor mm2, mm3    ; start restoring sign
  psubusw mm3, mm5
  psubw mm2, mm3   ; finish restoring sign

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

  ret

