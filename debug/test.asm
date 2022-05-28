SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    


EntryPoint:
	 ld hl, $FF41
	 jp statJump

printSerialCharacter:
	push hl 
	ld hl, $FF01		; Load SB address
	ld [hl], a			; Write character to SB
	ld hl, $FFO2		; Load SC address
	ld [hl], $81		; Request print
	pop hl
	ret

statJump:
	ld a, [hl]
	call printSerialCharacter
	jp statJump


