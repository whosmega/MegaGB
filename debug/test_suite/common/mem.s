memcpy:
    /* BC : Source
       DE : Destination
       A  : Size - 1
    */

    push bc
    push de
    push af
    push hl

.loop:
    push af

    LOAD_R16 h,l, b,l
    ld a, [hl]
    inc bc

    LOAD_R16 h,l, d,e
    ld [hl], a
    inc de

    pop af
    
    cp a, 0
    jp z, .end 

    dec a
    jp .loop

.end:
    pop hl
    pop af 
    pop de
    pop bc
    ret

; ===========================================================================

memfill:
    /* B : Byte to fill
       DE : Destination
       A : Size - 1
    */
    
    push af
    push hl
    LOAD_R16 h,l, d,e

.loop:
    push af

    ld a, b
    ld [hl+], a
    
    pop af

    cp a, 0
    jp z, .endloop

    dec a
    jp .loop

.endloop:
    pop hl
    pop af
    ret
