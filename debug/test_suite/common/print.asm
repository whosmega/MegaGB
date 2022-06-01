SECTION "print_asm", ROM0

; ------------------------------------------------------
printStringNL::         ; Print string with newline character 
    push af
    call printString
    ld a, "\n"
    call printChar
    pop af
    ret
; ------------------------------------------------------
printString::
    ; String pointer in HL
    ; Prints till it encounters null character
    push af

.loop:
    ld a, [hl]          ; Load char from address
    cp a, 0             ; check if its null pointer 
    jp z, .end          ; if it is, end the loop
    call printChar      ; otherwise print it
    inc hl              ; increment the pointer
    jp .loop            ; loop again

.end
    pop af
    ret
; ------------------------------------------------------ 
printChar::
	push hl 
	ld hl, R_SB	    	; Load SB address
	ld [hl], a			; Write character to SB
	ld hl, R_SC	    	; Load SC address
	ld [hl], $81		; Request print
	pop hl
	ret
; ------------------------------------------------------


EXPORT printChar, printString, printStringNL
