# *  - Inverse Discrete Cosine Transformation - PPC port
# *
# *  Copyright (C) 2002 Guillaume Morin <guillaume@morinfr.org>, Alc�ve
# *
# *  This file is part of XviD, a free MPEG-4 video encoder/decoder
# *
# *  XviD is free software; you can redistribute it and/or modify it
# *  under the terms of the GNU General Public License as published by
# *  the Free Software Foundation; either version 2 of the License, or
# *  (at your option) any later version.
# *
# *  This program is distributed in the hope that it will be useful,
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *  GNU General Public License for more details.
# *
# *  You should have received a copy of the GNU General Public License
# *  along with this program; if not, write to the Free Software
# *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# *
# *  Under section 8 of the GNU General Public License, the copyright
# *  holders of XVID explicitly forbid distribution in the following
# *  countries:
# *
# *    - Japan
# *    - United States of America
# *
# *  Linking XviD statically or dynamically with other modules is making a
# *  combined work based on XviD.  Thus, the terms and conditions of the
# *  GNU General Public License cover the whole combination.
# *
# *  As a special exception, the copyright holders of XviD give you
# *  permission to link XviD with independent modules that communicate with
# *  XviD solely through the VFW1.1 and DShow interfaces, regardless of the
# *  license terms of these independent modules, and to copy and distribute
# *  the resulting combined work under terms of your choice, provided that
# *  every copy of the combined work is accompanied by a complete copy of
# *  the source code of XviD (the version of XviD used to produce the
# *  combined work), being distributed under the terms of the GNU General
# *  Public License plus this exception.  An independent module is a module
# *  which is not derived from or based on XviD.
# *
# *  Note that people who make modified versions of XviD are not obligated
# *  to grant this special exception for their modified versions; it is
# *  their choice whether to do so.  The GNU General Public License gives
# *  permission to release a modified version without this exception; this
# *  exception also makes it possible to release a modified version which
# *  carries forward this exception.
# *
# * $Id: idct_altivec.s,v 1.5 2002-11-16 23:57:26 edgomez Exp $#

	
	.file	"idct_vec_tmpl.c"
gcc2_compiled.:
	.section	".data"
	.align 4
	.type	 PreScale,@object
	.size	 PreScale,128
PreScale:
	.long 268375601
	.long 350687952
	.long 268374736
	.long 350688817
	.long 372317896
	.long 486414872
	.long 372316696
	.long 486416072
	.long 350690558
	.long 458234004
	.long 350689428
	.long 458235134
	.long 315628056
	.long 412358175
	.long 315627039
	.long 412359192
	.long 268375601
	.long 350687952
	.long 268374736
	.long 350688817
	.long 315628056
	.long 412358175
	.long 315627039
	.long 412359192
	.long 350690558
	.long 458234004
	.long 350689428
	.long 458235134
	.long 372317896
	.long 486414872
	.long 372316696
	.long 486416072
	.section	".rodata"
	.align 4
.LC0:
	.long 1518482693
	.long 427185543
	.long -1518425479
	.long 0
	.section	".text"
	.align 2
	.globl idct_altivec
	.type	 idct_altivec,@function
idct_altivec:
	stwu 1,-368(1)
	stw 15,300(1)
	stw 16,304(1)
	stw 17,308(1)
	stw 18,312(1)
	stw 19,316(1)
	stw 20,320(1)
	stw 21,324(1)
	stw 22,328(1)
	stw 23,332(1)
	stw 24,336(1)
	stw 25,340(1)
	stw 26,344(1)
	stw 27,348(1)
	stw 28,352(1)
	stw 29,356(1)
	stw 30,360(1)
	stw 31,364(1)
	addi 0,0,272
	stvx 31,1,0
	lis 11,PreScale@ha
	lvx 12,0,3
	vxor %v3,%v3,%v3
	la 11,PreScale@l(11)
	lvx 0,0,11
	addi 9,11,16
	lvx 1,0,9
	addi 17,3,32
	addi 9,11,32
	lvx 8,0,17
	lvx 7,0,9
	addi 15,3,16
	vmhraddshs 12,12,0,3
	addi 9,11,48
	lvx 9,0,15
	addi 16,3,48
	lvx 0,0,9
	lvx 11,0,16
	addi 9,11,64
	addi 18,3,64
	lvx 10,0,9
	vmhraddshs 8,8,7,3
	lvx 13,0,18
	addi 9,11,80
	vmhraddshs 9,9,1,3
	lvx 7,0,9
	addi 19,3,80
	vmhraddshs 11,11,0,3
	addi 20,3,96
	lvx 1,0,19
	lvx 0,0,20
	addi 4,1,16
	addi 9,11,96
	vmhraddshs 13,13,10,3
	stvx 12,0,4
	addi 11,11,112
	lvx 10,0,9
	vmhraddshs 1,1,7,3
	lvx 12,0,11
	addi 5,1,32
	stvx 9,0,5
	addi 21,3,112
	lvx 5,0,21
	addi 7,1,48
	vmhraddshs 0,0,10,3
	stvx 8,0,7
	addi 8,1,64
	stvx 11,0,8
	addi 6,1,80
	vmhraddshs 5,5,12,3
	stvx 13,0,6
	addi 11,1,96
	stvx 1,0,11
	addi 10,1,112
	stvx 0,0,10
	addi 27,1,128
	stvx 5,0,27
	lvx 1,0,11
	lis 9,.LC0@ha
	lvx 12,0,5
	la 9,.LC0@l(9)
	lvx 0,0,6
	addi 25,1,144
	lvx 2,0,10
	addi 23,1,256
	lvx 11,0,4
	addi 26,1,160
	lvx 6,0,7
	vmrghh 10,12,1
	addi 24,1,240
	lvx 7,0,8
	vmrglh 12,12,1
	addi 29,1,176
	lvx 4,0,9
	addi 28,1,224
	vmrghh 13,11,0
	addi 9,1,192
	vmrghh 1,6,2
	addi 22,1,208
	vmrghh 9,7,5
	vmrglh 11,11,0
	vmrghh 8,10,9
	vmrghh 0,13,1
	vmrglh 13,13,1
	vmrghh 1,0,8
	vmrglh 6,6,2
	stvx 1,0,4
	vmrglh 7,7,5
	vmrglh 10,10,9
	vmrglh 0,0,8
	stvx 0,0,5
	vmrghh 9,12,7
	vmrghh 1,11,6
	vmrghh 0,13,10
	stvx 0,0,7
	vmrglh 13,13,10
	stvx 13,0,8
	vmrglh 12,12,7
	vmrghh 0,1,9
	vmrglh 11,11,6
	stvx 0,0,6
	vmrglh 1,1,9
	stvx 1,0,11
	vmrghh 0,11,12
	stvx 0,0,10
	vmrglh 11,11,12
	stvx 11,0,27
	vsplth 19,4,2
	lvx 0,0,5
	vsplth 17,4,5
	lvx 10,0,11
	vsplth 16,4,3
	lvx 1,0,8
	vsplth 18,4,1
	lvx 13,0,7
	vsplth 2,4,0
	vmhraddshs 15,19,0,3
	lvx 9,0,10
	vsplth 4,4,4
	vmhraddshs 5,19,11,0
	lvx 12,0,4
	vmhraddshs 8,17,1,10
	lvx 0,0,6
	vmhraddshs 6,16,10,1
	vmhraddshs 14,18,9,13
	vsubshs 1,15,11
	vmhraddshs 15,18,13,3
	vaddshs 31,1,8
	vsubshs 8,1,8
	vsubshs 1,5,6
	vaddshs 7,12,0
	vaddshs 5,5,6
	vaddshs 6,7,14
	vsubshs 11,15,9
	vsubshs 10,12,0
	vsubshs 14,7,14
	vaddshs 7,10,11
	vsubshs 10,10,11
	vsubshs 11,1,8
	vaddshs 8,1,8
	vaddshs 0,6,5
	stvx 0,0,25
	vmhraddshs 13,2,8,7
	vsubshs 1,6,5
	stvx 1,0,23
	vmhraddshs 12,4,8,7
	stvx 13,0,26
	vmhraddshs 0,2,11,10
	stvx 12,0,24
	vmhraddshs 13,4,11,10
	stvx 0,0,29
	vaddshs 1,14,31
	stvx 13,0,28
	vsubshs 0,14,31
	stvx 1,0,9
	stvx 0,0,22
	lvx 9,0,28
	lvx 12,0,26
	lvx 8,0,24
	lvx 5,0,23
	lvx 11,0,25
	lvx 6,0,29
	vmrghh 10,12,9
	lvx 7,0,9
	vmrglh 12,12,9
	vmrghh 1,11,0
	vmrghh 13,6,8
	vmrghh 9,7,5
	vmrglh 6,6,8
	vmrglh 11,11,0
	vmrghh 8,10,9
	vmrghh 0,1,13
	vmrglh 1,1,13
	vmrghh 13,0,8
	stvx 13,0,4
	vmrglh 7,7,5
	vmrglh 10,10,9
	vmrglh 0,0,8
	stvx 0,0,5
	vmrghh 9,12,7
	vmrghh 13,11,6
	vmrghh 0,1,10
	stvx 0,0,7
	vmrglh 1,1,10
	vmrglh 12,12,7
	stvx 1,0,8
	vmrghh 0,13,9
	vmrglh 11,11,6
	stvx 0,0,6
	vmrglh 13,13,9
	stvx 13,0,11
	vmrghh 0,11,12
	stvx 0,0,10
	vmrglh 11,11,12
	stvx 11,0,27
	lvx 0,0,5
	lvx 13,0,11
	lvx 1,0,8
	lvx 12,0,7
	vmhraddshs 15,19,0,3
	lvx 9,0,10
	vmhraddshs 5,19,11,0
	lvx 10,0,4
	vmhraddshs 8,17,1,13
	lvx 0,0,6
	vmhraddshs 6,16,13,1
	vmhraddshs 14,18,9,12
	vsubshs 1,15,11
	vmhraddshs 15,18,12,3
	vaddshs 31,1,8
	vsubshs 8,1,8
	vsubshs 1,5,6
	vaddshs 7,10,0
	vaddshs 5,5,6
	vaddshs 6,7,14
	vsubshs 10,10,0
	vsubshs 11,15,9
	vsubshs 14,7,14
	vaddshs 7,10,11
	vsubshs 10,10,11
	vsubshs 11,1,8
	vaddshs 8,1,8
	vaddshs 0,6,5
	stvx 0,0,25
	vmhraddshs 13,2,8,7
	vsubshs 1,6,5
	stvx 1,0,23
	vmhraddshs 0,4,8,7
	stvx 13,0,26
	vmhraddshs 2,2,11,10
	stvx 0,0,24
	vmhraddshs 4,4,11,10
	stvx 2,0,29
	vaddshs 0,14,31
	stvx 4,0,28
	vsubshs 12,14,31
	stvx 0,0,9
	stvx 12,0,22
	lvx 0,0,25
	stvx 0,0,3
	lvx 1,0,26
	stvx 1,0,15
	lvx 0,0,29
	stvx 0,0,17
	lvx 13,0,28
	lvx 1,0,9
	stvx 1,0,16
	lvx 0,0,23
	stvx 12,0,18
	lvx 1,0,24
	stvx 13,0,19
	stvx 1,0,20
	stvx 0,0,21
	lwz 15,300(1)
	lwz 16,304(1)
	lwz 17,308(1)
	lwz 18,312(1)
	lwz 19,316(1)
	lwz 20,320(1)
	lwz 21,324(1)
	lwz 22,328(1)
	lwz 23,332(1)
	lwz 24,336(1)
	lwz 25,340(1)
	lwz 26,344(1)
	lwz 27,348(1)
	lwz 28,352(1)
	lwz 29,356(1)
	lwz 30,360(1)
	lwz 31,364(1)
	addi 0,0,272
	lvx 31,1,0
	la 1,368(1)
	blr
.Lfe1:
	.size	 idct_altivec,.Lfe1-idct_altivec
	.ident	"GCC: (GNU) 2.95.3 20010111 (BLL/AltiVec prerelease/franzo/20010111)"
