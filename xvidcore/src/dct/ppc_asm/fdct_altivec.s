# *  - Forward Discrete Cosine Transformation - PPC port
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
# * $Id: fdct_altivec.s,v 1.4 2002-11-16 23:57:26 edgomez Exp $#
	
	.file	"dct_vec_tmpl.c"
gcc2_compiled.:
	.globl PostScale
	.section	".data"
	.align 4
	.type	 PostScale,@object
	.size	 PostScale,128
PostScale:
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
	.long 1518492290
	.long 1518492290
	.long 1518492290
	.long 1518492290
	.align 4
.LC1:
	.long 889533701
	.long 889533701
	.long 889533701
	.long 889533701
	.align 4
.LC2:
	.long 427170166
	.long 427170166
	.long 427170166
	.long 427170166
	.align 4
.LC3:
	.long 1434932615
	.long 1434932615
	.long 1434932615
	.long 1434932615
	.align 4
.LC4:
	.long -1518426754
	.long -1518426754
	.long -1518426754
	.long -1518426754
	.align 4
.LC5:
	.long -1434867079
	.long -1434867079
	.long -1434867079
	.long -1434867079
	.section	".text"
	.align 2
	.globl fdct_altivec
	.type	 fdct_altivec,@function
fdct_altivec:
	stwu 1,-368(1)
	mflr 0
	stw 14,296(1)
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
	stw 0,372(1)
	addi 0,0,272
	stvx 31,1,0
	lvx 0,0,3
	addi 23,1,16
	vxor %v3,%v3,%v3
	addi 18,3,16
	stvx 0,0,23
	lvx 1,0,18
	addi 24,1,32
	addi 17,3,32
	stvx 1,0,24
	lvx 0,0,17
	addi 25,1,48
	addi 20,3,48
	stvx 0,0,25
	lvx 1,0,20
	addi 26,1,64
	addi 19,3,64
	stvx 1,0,26
	lvx 0,0,19
	addi 28,1,80
	addi 21,3,80
	stvx 0,0,28
	lvx 1,0,21
	addi 27,1,96
	addi 22,3,96
	stvx 1,0,27
	lvx 0,0,22
	addi 7,1,112
	stvx 0,0,7
	addi 16,3,112
	lvx 1,0,16
	addi 14,1,128
	stvx 1,0,14
	lvx 0,0,23
	lis 11,.LC0@ha
	lvx 12,0,26
	la 11,.LC0@l(11)
	lvx 13,0,28
	lis 8,.LC1@ha
	lvx 11,0,7
	la 8,.LC1@l(8)
	lvx 9,0,24
	vaddshs 6,0,1
	lis 10,.LC4@ha
	vsubshs 5,0,1
	lvx 10,0,25
	la 10,.LC4@l(10)
	lvx 0,0,27
	vaddshs 7,12,13
	lis 9,.LC2@ha
	vsubshs 31,12,13
	lvx 15,0,11
	addi 29,1,144
	vaddshs 13,9,11
	lvx 19,0,8
	la 9,.LC2@l(9)
	vsubshs 9,9,11
	lvx 16,0,10
	addi 4,1,208
	vaddshs 12,10,0
	lvx 2,0,9
	lis 11,.LC3@ha
	vsubshs 4,6,7
	la 11,.LC3@l(11)
	vaddshs 11,6,7
	lis 9,.LC5@ha
	lvx 14,0,11
	vsubshs 10,10,0
	addi 6,1,176
	vsubshs 6,13,12
	la 9,.LC5@l(9)
	vaddshs 7,13,12
	lvx 17,0,9
	addi 5,1,240
	vaddshs 12,9,10
	addi 10,1,160
	vsubshs 1,11,7
	addi 8,1,256
	vaddshs 0,11,7
	lis 9,PostScale@ha
	vmhraddshs 11,12,15,5
	stvx 0,0,29
	vsubshs 13,9,10
	stvx 1,0,4
	vmhraddshs 1,4,19,3
	la 9,PostScale@l(9)
	vmhraddshs 10,13,15,31
	addi 11,1,224
	vmhraddshs 7,12,16,5
	addi 15,1,192
	vmhraddshs 5,11,2,3
	addi 0,9,16
	vmhraddshs 9,13,16,31
	mtctr 0
	addi 30,9,32
	vmhraddshs 0,6,19,4
	addi 0,9,48
	vmhraddshs 13,10,2,11
	addi 31,9,64
	vsubshs 1,1,6
	stvx 0,0,6
	mtlr 31
	stvx 1,0,5
	vsubshs 0,5,10
	stvx 13,0,10
	vmhraddshs 12,7,14,9
	stvx 0,0,8
	vmhraddshs 6,9,17,7
	lvx 18,0,9
	addi 12,9,80
	stvx 12,0,11
	stvx 6,0,15
	lvx 13,0,4
	addi 31,9,96
	lvx 12,0,29
	addi 9,9,112
	lvx 0,0,11
	lvx 4,0,5
	lvx 5,0,8
	lvx 11,0,10
	vmrghh 1,12,13
	lvx 9,0,6
	vmrglh 12,12,13
	vmrghh 8,6,5
	vmrghh 10,11,0
	vmrghh 13,9,4
	vmrglh 11,11,0
	vmrghh 7,10,8
	vmrghh 0,1,13
	vmrglh 1,1,13
	vmrghh 13,0,7
	stvx 13,0,23
	vmrglh 9,9,4
	vmrglh 6,6,5
	vmrglh 10,10,8
	vmrglh 0,0,7
	stvx 0,0,24
	vmrghh 8,11,6
	vmrghh 13,12,9
	vmrghh 0,1,10
	stvx 0,0,25
	vmrglh 1,1,10
	stvx 1,0,26
	vmrglh 11,11,6
	vmrghh 0,13,8
	vmrglh 12,12,9
	stvx 0,0,28
	vmrglh 13,13,8
	stvx 13,0,27
	vmrghh 0,12,11
	stvx 0,0,7
	vmrglh 12,12,11
	stvx 12,0,14
	lvx 0,0,23
	lvx 11,0,26
	lvx 13,0,28
	lvx 8,0,24
	lvx 9,0,7
	vaddshs 6,0,12
	lvx 1,0,27
	vsubshs 5,0,12
	mfctr 7
	lvx 10,0,25
	vaddshs 7,11,13
	vsubshs 31,11,13
	vaddshs 13,8,9
	vsubshs 4,6,7
	vaddshs 12,10,1
	vaddshs 11,6,7
	vsubshs 9,8,9
	vsubshs 10,10,1
	vsubshs 6,13,12
	vaddshs 7,13,12
	vaddshs 12,9,10
	vsubshs 1,11,7
	vaddshs 0,11,7
	vmhraddshs 11,12,15,5
	stvx 0,0,29
	vsubshs 13,9,10
	stvx 1,0,4
	vmhraddshs 1,4,19,3
	vmhraddshs 10,13,15,31
	vmhraddshs 7,12,16,5
	vmhraddshs 9,13,16,31
	vmhraddshs 5,11,2,3
	vmhraddshs 19,6,19,4
	vsubshs 0,1,6
	stvx 19,0,6
	vmhraddshs 2,10,2,11
	stvx 0,0,5
	vmhraddshs 14,7,14,9
	stvx 2,0,10
	vsubshs 0,5,10
	vmhraddshs 17,9,17,7
	stvx 0,0,8
	stvx 14,0,11
	stvx 17,0,15
	lvx 0,0,29
	vmhraddshs 18,18,0,3
	stvx 18,0,3
	lvx 1,0,7
	lvx 0,0,10
	mflr 7
	lvx 11,0,11
	lvx 13,0,6
	mr 11,0
	lvx 12,0,4
	vmhraddshs 1,1,0,3
	lvx 10,0,5
	lvx 9,0,8
	stvx 1,0,18
	lvx 0,0,30
	vmhraddshs 0,0,13,3
	stvx 0,0,17
	lvx 1,0,11
	vmhraddshs 1,1,17,3
	stvx 1,0,20
	lvx 0,0,7
	vmhraddshs 0,0,12,3
	stvx 0,0,19
	lvx 1,0,12
	vmhraddshs 1,1,11,3
	stvx 1,0,21
	lvx 0,0,31
	vmhraddshs 0,0,10,3
	stvx 0,0,22
	lvx 12,0,9
	vmhraddshs 12,12,9,3
	stvx 12,0,16
	lvx 0,0,19
	lvx 11,0,3
	lvx 1,0,21
	lvx 10,0,18
	lvx 6,0,17
	lvx 4,0,22
	vmrghh 13,11,0
	lvx 8,0,20
	vmrglh 11,11,0
	vmrghh 9,10,1
	vmrglh 10,10,1
	vmrghh 0,6,4
	vmrghh 7,8,12
	vmrghh 1,13,0
	vmrghh 5,9,7
	vmrglh 13,13,0
	vmrghh 0,1,5
	stvx 0,0,3
	vmrglh 8,8,12
	vmrglh 6,6,4
	vmrglh 9,9,7
	vmrglh 1,1,5
	stvx 1,0,18
	vmrghh 12,11,6
	vmrghh 1,10,8
	vmrghh 0,13,9
	stvx 0,0,17
	vmrglh 13,13,9
	stvx 13,0,20
	vmrghh 0,12,1
	vmrglh 11,11,6
	stvx 0,0,19
	vmrglh 10,10,8
	vmrglh 12,12,1
	stvx 12,0,21
	vmrghh 0,11,10
	stvx 0,0,22
	vmrglh 11,11,10
	stvx 11,0,16
	lwz 0,372(1)
	mtlr 0
	lwz 14,296(1)
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

