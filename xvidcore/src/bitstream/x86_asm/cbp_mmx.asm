;/**************************************************************************
; *
; *	XVID MPEG-4 VIDEO CODEC
; *	mmx cbp calc
; *
; *	This program is an implementation of a part of one or more MPEG-4
; *	Video tools as specified in ISO/IEC 14496-2 standard.  Those intending
; *	to use this software module in hardware or software products are
; *	advised that its use may infringe existing patents or copyrights, and
; *	any such use would be at such party's own risk.  The original
; *	developer of this software module and his/her company, and subsequent
; *	editors and their companies, will have no liability for use of this
; *	software or modifications or derivatives thereof.
; *
; *	This program is free software; you can redistribute it and/or modify
; *	it under the terms of the GNU General Public License as published by
; *	the Free Software Foundation; either version 2 of the License, or
; *	(at your option) any later version.
; *
; *	This program is distributed in the hope that it will be useful,
; *	but WITHOUT ANY WARRANTY; without even the implied warranty of
; *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; *	GNU General Public License for more details.
; *
; *	You should have received a copy of the GNU General Public License
; *	along with this program; if not, write to the Free Software
; *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
; *
; *************************************************************************/

;/**************************************************************************
; *
; *	History:
; *
; *	14.06.2002	some further tweaks by -Skal-
; *	24.11.2001	inital version; (c)2001 peter ross <pross@cs.rmit.edu.au>
; *
; *************************************************************************/


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

align 16

ignore_dc	dw		0, -1, -1, -1, -1, -1, -1, -1

section .text

cglobal calc_cbp_mmx

;===========================================================================
;
; uint32_t calc_cbp_mmx(const int16_t coeff[6][64]);
;
;===========================================================================

align 16
calc_cbp_mmx:
  push	ebx
  push	esi

  mov       esi, [esp + 8 + 4]	; coeff
  xor		eax, eax			; cbp = 0
  mov		edx, (1 << 5)

  movq		mm7, [ignore_dc]

.loop
  movq		mm0, [esi]
  movq		mm1, [esi+8]
  pand		mm0, mm7

  por     	mm0, [esi+16]
  por     	mm1, [esi+24]

  por     	mm0, [esi+32]
  por     	mm1, [esi+40]

  por     	mm0, [esi+48]
  por     	mm1, [esi+56]

  por     	mm0, [esi+64]
  por     	mm1, [esi+72]

  por     	mm0, [esi+80]
  por     	mm1, [esi+88]

  por     	mm0, [esi+96]
  por     	mm1, [esi+104]

  por     	mm0, [esi+112]
  por     	mm1, [esi+120]

  por     	mm0, mm1
  movq    	mm1, mm0
  psrlq   	mm1, 32
  lea		esi, [esi + 128]

  por     	mm0, mm1
  movd		ebx, mm0

  test 		ebx, ebx
  jz		.next
  or 		eax, edx     ; cbp |= 1 << (5-i)

.next
  shr 		edx,1
  jnc		.loop

  pop		esi
  pop		ebx
				
  ret