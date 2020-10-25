; 
	.area	_HEADER (ABS)
	.org 	0

	di
	jp	init

;
	.org	4
	.db	0xbf, 0x1b
	.db	0xbe, 0xbe

; RDSLT
	.org	0xc
	xor	a
	ret

; WRTSLT
	.org	0x14
	ret

; CALSLT
	.org	0x1c
	ret

; ENASLT
	.org	0x24
	ret

; CALLF
	.org	0x30
	ret

; RSLREG
	.org	0x138
	xor	a
	ret

; WSLREG
	.org	0x13B
	ret

; BREAKX
	.org	0x46f
	or	a, a
	ret
	
; readkey
	.org	0xd12
	or	a, #1
	ret

; read row 8
	.org	0x1226
	ld	a, #255
	ei
	ret
	
; SNSMAT
	.org	0x141
;	ld	a, #0xff
;	ret
	jp	snsmat_emu

; RDSLT
	.org	0x1b6
	di
	ld	a, (hl)
	ret

;font
	.org	0x727

	ld	hl, (#0xf920)	; CGFNTADDR
	ld	de, #0
	ld	bc, #0x1800
	
70$:
	push	hl
	add	hl, de
	ld	a, (hl)
	pop	hl

	out	(#0xbe), a
	inc	de
	ld	a, d
	and	a, #0x07
	ld	d, a
	dec	bc
	ld	a, c
	or	b
	jr	NZ, 70$
	ret

; CALBAS
	.org	0x1ff
	jp	(ix)

; SNSMAT
	.org	0x1452
snsmat_emu:
	push	hl
	add	a, #<keyBuf
	ld	l, a
	ld	h, #>keyBuf
	ld	a, (hl)
	pop	hl
	ret

; NMI
	.org     0x66
	jp	startButton

	.org	0x38
	jp	intSub

; 0x2d7-0x3f8 
	.org	 0x2d7

init:
	im	1
	ld sp, #0xdff0

	; init mapper
	ld hl, #0xfffc
	ld (hl), #0
	inc l
	ld (hl), #0
	inc l
	ld (hl), #1
	inc l
	ld (hl), #2

	; clear memory
	ld hl, #0xc000
	ld (hl), #0
	ld de, #0xc001
	ld bc, #0x1fef
	ldir
        		
	; initPSG
	ld	hl, #psgWorkData
	ld	de, #psgWork
	ld	bc, #(16 + 3)
	ldir

	call	clearKeyBuf

	ld	a, (#0xbf)	; reset flag
	
	; initPalette
	ld	de, #0xc000
	ld	hl, #paletteData
	ld	b, #32
	ld	c, #0xbf
60$:
	out	(c), e
	out	(c), d
	ld	a, (hl)
	out	(#0xbe), a
	inc	hl
	inc	de
	djnz	60$
	
	jp	0x7c76

clearKeyBuf:
	ld	hl, #keyBuf
	ld	(hl), #0xff
	ld	de, #(keyBuf+1)
	ld	bc, #8
	ldir
	ret

psgWorkData:
	.db	0, 0, 0, 0, 0, 0,  0, 0xff, 0, 0, 0,  0,0,0,0,0, 0,0,0

paletteData:
	.db	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
	.db	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0

startButton:
	push	af
	ld	a, #6	; 1/10 sec
	ld	(#startButtonLeftFrames), a
	
;	ld	a, #0b11110101
;	ld	a, #0b01010101
;	out	(#0x3f), a

	pop	af
	retn

intSub:
	call	0xc3c
	
	push	af
	push	hl
	push	bc
	push	de
	
	call	clearKeyBuf
	
	ld	a, (#startButtonLeftFrames)
	or	a
	jr	Z, 1$
	dec	a
	ld	(#startButtonLeftFrames), a
	
	ld	a, #0x7f
	ld	(#(keyBuf + 7)), a
	
	nop
	nop
	nop
	nop
	nop
	
	nop
	nop
	nop
	nop
	nop
	
1$:
	pop	de
	pop	bc
	pop	hl
	pop	af
	ret


jump2rom:
	ld	hl, #0xfffe
	ld	a, #2
	ld 	(hl), a
	inc	a
	inc	hl
	ld	(hl), a

	ld	hl,(0x4002)

	jp	(hl)

; TAPOON から TAPINまで
; 19f1-1b42 : 337byte
	.org	0x19f1

; A	PSGのレジスタ番号
; E	書き込むデータ
wrtpsg_emu:
	push	af
	push	de
	push	hl
	push	bc
	
	ld	d, a		; d = reg
	
	; h = prevChStat
	ld	a, (#(psgWork + 7))
	ld	h, a
	
	; psgWork[a] = e
	ld	a, #<psgWork
	add	a, d
	ld	c, a
	ld	b, #>psgWork	; 下位8bitまたがない
	ld	a, #0
	adc	a, #>psgWork
	ld	b, a
	ld 	a, e
	ld	(bc), a

	; if (reg == 7)
	ld	a, d
	sub	a, #7
	jp	NZ, wrtpsg_emu_5
	
	; changed = prevChStat ^ data
	ld	a, h
	xor	e
	ld	d, a		; d : changed
	
	;for (i = 0; i < 3; ++i)
	ld	b, #0		; b : i
3$:
	; v = curVol_[i]
	ld	a,#<volWork
	add	a, b
	ld	l, a
	ld	h, #>volWork	; またがない
	ld	c, (hl)		; c : vol
	
	; tone
	; if (changed & 1)
	bit	0, d
	jr	Z, 4$
	
	; i << 5
	ld	a, b
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	
	; if (data & 1)
	bit	0, e
	jr 	Z, 5$
	
	; keyoff
	or	a, #0x9f
	out	(#0x7f), a
	jr	4$
5$:
	; else
	; keyon
	or	a, c
	or	a, #0x90
	out	(#0x7f), a

4$:
	; noise
	; if (chaged & 8)
	bit	3, d
	jr	Z, 6$
	
	; if (!(data & 8))
	bit	3, e
	jr	NZ, 6$
	
	; noise on
	ld	a, c	; vol
	or	a, #0xf0
	out	(#0x7f), a

6$:
	; data >>= 1; changed >>= 1;
	srl	e
	srl	d

	; loop tail
	inc	b
	ld	a, b
	sub	a, #3
	jr	C, 3$

	; if ((changed & 7) && (data & 7 == 7))
	ld	a, d
	and	a, #7
	jp	Z, wrtpsg_emu_2
	ld	a, e
	and	a, #7
	sub	a, #7
	jp	NZ, wrtpsg_emu_2
	
	; noise off
	ld	a, #0xff
	out	(#0x7f), a
	jp	NZ, wrtpsg_emu_2
	
wrtpsg_emu_5:
	; else if (reg == 6)
	ld	a, d	; reg
	sub	a, #6
	jr	NZ, 10$
	
	; noise freq
	ld	a, e
	rrca
	rrca
	rrca
	and	a, #3
	ld	c, a
	sub	a, #3
	jr	NZ, 11$
	ld	c, #2
11$:
	ld	a, #0xe4
	or	a, c
	out	(#0x7f), a
	jp	NZ, wrtpsg_emu_2
	
	
10$:
	; note
	; d = reg
	; if (!(reg & 1) && reg < 6)
	bit	0, d
	jr	NZ, wrtpsg_emu_1
;	jr	Z, wrtpsg_emu_1
	ld	a, d
	sub	a, #6
	jr 	NC, wrtpsg_emu_1
	;
	ld	a, d
	and	a, #6
	ld	e, a	; e = reg & 6 = rbase
	
	ld	b, #0
	ld	c, a
	ld	hl, #psgWork
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)	; bc = tune

	; while(v >= 1 << 10) v >>= 1;
1$:
	ld	a, b
	sub	a, #4
	jr	C, 2$
	srl	b
	rr	c
	jr	1$
2$:
	; out(0x40, 0x80 | (rbase << 4) | (tune & 15))
	ld	a, e
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	set	7, a
	ld	e, a
	ld	a, c
	and	a, #15
	or	a, e
;	di
	out	(#0x7f), a

	; out(0x40, (tune >> 4) & 63)
	srl	b
	rr	c
	srl	b
	rr	c
	srl	b
	rr	c
	srl	b
	rr	c
	ld	a, c
	and	a, #63
	out	(#0x7f), a
;	ei
	jr	wrtpsg_emu_2

wrtpsg_emu_1:
	; else
	; volume
	; regv = reg - 8
	ld	a, d
	add	a, #-8
	ld	d, a		; d : regv

	; if (regv < 3)
	sub	a, #3
	jr	NC, wrtpsg_emu_3
	
	; chstat = psgWork_[7] >> regv
	ld	a, (#(psgWork + 7))
	ld	c, d
	inc	c
	jr	12$
13$:
	rra	; rotで構わない
12$:
	dec	c
	jr	NZ, 13$
	ld	c, a		; c : chstat >> regv
	
	; v = data ^ 15
	ld	a, e
	xor	a, #15
	bit	4, a
	jr	Z, wrtpsg_emu_4
	xor	a
	
wrtpsg_emu_4:
	; curVol[regv] = v
	ld	e, a		; e : v

	ld	a,#<volWork
	add	a, d		; +regv
	ld	l, a
	ld	h, #>volWork	; またがない
	ld	(hl), e
	
	; v |= 0x90 | (regv << 5);
	ld	a, d
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	or	a, #0x90
	or	a, e		; a : v
	
	; if (!(chstat & 1))
	bit	0, c
	jr	NZ, 15$
	out	(#0x7f), a
15$:
	; if (!(chstat & 8))
	bit	3, c
	jr	NZ, 16$
	or	a, #0x60
	out	(#0x7f), a
16$:
	
wrtpsg_emu_3:
wrtpsg_emu_2:
	pop	bc
	pop	hl
	pop	de
	pop	af
	ret

;
	.org	0x37fe
pageset_ofs:
	.db	0
pageset_count:
	.db	0
	.org	0x3800
bank1_table:
	.org	0x3900
bank2a_table:
	.org	0x3a00
bank2b_table:
	.org	0x3b00
pageset_bank2_table:
	
;;;;;;;
	.org	0x3c00
change_bank1:
	push	hl
;	push	bc
;	push	de
	push	af
	
	ld	(#cur_bank1), a
	ld	l, a
	ld	h, #>bank1_table
	ld	a, (hl)
	ld	(#0xfffe), a
	
	pop	af
;	pop	de
;	pop	bc
	pop	hl

	ret


change_bank2:
	push	hl
	push	bc
	push	de
	push	af
	
	ld	(#cur_bank2), a
	ld	d, a
	add	a, a
	ld	l, a
	ld	h, #>bank2a_table
	ld	b, (hl)	; sms page
	inc	hl
	ld	c, (hl)	; msx page3
	ld	a, (#cur_bank3)
	ld	e, a
	sub	a, c
change_bank2_3:
	jr	Z, 100$
	
	; 一致しない
	push	bc
	call	find_page_pair
	pop	bc
	
	or	a, a
	jr	Z, 100$
	ld	b, a
100$:
	; b = sms page
	ld	a, b
	ld	(#0xffff), a
	
	pop	af
	pop	de
	pop	bc
	pop	hl

	ret


change_bank3:
	push	hl
	push	bc
	push	de
	push	af
	
	ld	(#cur_bank3), a
	ld	e, a
	add	a, a
	ld	l, a
	ld	h, #>bank2b_table
	ld	b, (hl)	; sms page
	inc	hl
	ld	c, (hl)	; msx page2
	ld	a, (#cur_bank2)
	ld	d, a
	sub	a, c
	jr	change_bank2_3



find_page_pair:
	; out:
	; 	a にsms page
	; in:
	;	de にmsx page 3,2
	
	ld	a, (#pageset_count)
	ld	c, a	; right
	xor	a
	ld	b, a	; left
	
	; while(left < right)
109$:
	ld	a, b
	cp	c
	jr	NC, 110$
	
	add	a, c
	and	#0xfe	; a >> 1 << 1
	ld	l, a
	ld	h, #>pageset_bank2_table
	push	bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	srl	l	; mid
	
	ld	a, c
	cp	e
	jr	NZ, 111$
	ld	a, b
	cp	d
	jr	NZ, 111$
	
	; find!
	pop	bc
	ld	a, (#pageset_ofs)
	add	a, l
	ret
	
111$:
	; else if (de < bc)
	ld	a, e
	sub	a, c
	ld	a, d
	sbc	a, b
	pop	bc
	jr	NC, 112$
	
	; right = mid
	ld	c, l
	jr	109$
	
112$:
	; left = mid + 1
	ld	b, l
	inc	b
	jr	109$
	
110$:
	xor	a
	ret

;

; F55E:RAM :WORK:BUF 258bytes
	.org	0xf560
psgWork:

	.org	0xf570
volWork:

	.org	0xf580
keyBuf:
	; 9 bytes

	.org	0xf58f
startButtonLeftFrames:
	; 1 byte




	.org	0xf65c
cur_bank:
	.org	0xf65d
cur_bank1:
	.org	0xf65e
cur_bank2:
	.org	0xf65f
cur_bank3:




; WRTPSG
	.org	0x93
	jp	wrtpsg_emu

	.org	0x1102
	jp	wrtpsg_emu

; RDPSG -> joystick
	.org	0x110e
	in	a,(#0xc0)
	and	a, #63
	ret

;
	.org	0x7d5d
	ld	hl, #0xe000
	ret

;
	.org	0x7d75
	jp 	jump2rom
