SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    


EntryPoint:
    ; Testing WRAM Bank Switching in CGB mode
	ld b, $10
	ld c, $20
	ld d, $30

	ld a, 1
	ld [$FF70], a		; Switch to bank 1
	ld a, b
	ld [$DD00], a		; Store $10 at DD00
	
	ld a, 2
	ld [$FF70], a		; Switch to bank 2
	ld a, c
	ld [$DD00],	a		; Store $20 at DD00

	ld a, 3 
	ld [$FF70], a		; Switch to bank 3
	ld a, d
	ld [$DD00], a		; Store $30 at DD00

	ld a, 1
	ld [$FF70], a		; Switch to bank 1
	ld a, [$DD00]		
	ld e, a				; Store DD00 in e

	ld a, 2
	ld [$FF70], a		; Switch to bank 2
	ld a, [$DD00]
	ld h, a				; Store DD00 in h

	ld a, 3
	ld [$FF70], a		; Switch to bank 3
	ld a, [$DD00]		
	ld l, a				; Store DDOO in l

	ld b, b





