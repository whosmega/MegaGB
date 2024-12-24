INCLUDE "interface.inc"

pollVBlank:
    push af 
    push hl
    push bc

    ld hl, R_STAT
    ld b, 3
.loop:
    ld a, [hl]
    and a, b
    cp a, 1
    jp nz, .loop

    pop bc
    pop hl
    pop af
    ret 

; =============================================================================
initPalettes:
    /* index 0 = white (transparent for sprites)
       index 1 = light gray
       index 2 = dark gray
       index 3 = black */
    push hl

    LOAD_HREG R_BGP, $E4
    LOAD_HREG R_OBP0, $E4
    LOAD_HREG R_OBP1, $E4

    pop hl
    ret

loadTiles_8000h:
    /* 
        Loads tiles at $8000 (start of VRAM), max number of tiles it can 
        load is 255 (till $8FFF). If A < 255, it initialises remaining slots 
        to index 0 blank tiles. This is meant to be used with the $8000 addressing method

        Address of tile data sector in BC
        Number of tiles to load - 1 in A */

    push de
    push hl
    push af

    ld de, $8000                        ; Load destination

.loop:
    push af

    ld a, 15                            ; Load 16 bytes = 1 tile
    call memcpy                         ; Source and length in respective registers 
    pop af

    cp a, 0
    jp z, .endloop                      ; No more tiles to load

    dec a                               ; Reduce counter

    ld hl, 16                           ; Increase source pointer by 16
    add hl, bc
    LOAD_R16 b,c, h,l

    ld hl, 16                           ; Increase destination pointer by 16
    add hl, de
    LOAD_R16 d,e, h,l

    jp .loop

.endloop:
    pop af
    jp .endloop2
    /* 
        Basically A = 255 - A
        If A was 0, 1 tile was loaded. This preserves that aswell. So A = 255 - 0 = 255
        So 255/256 tiles need to be loaded with null tiles
    */

    cp a, 255
    jp z, .endloop2                     ; All tiles were loaded in, no null tiles

    push bc
    ld b, a
    ld a, 255
    sub a, b
    pop bc

    ld hl, 16
    add hl, de
    LOAD_R16 d,e, h,l

    ; DE contains pointer to the next destination
    ; A contains the remaining number of tiles to push

    ld b, 0                             ; We no longer need BC, use B to store fill byte
.loop2:
    cp a, 0
    jp z, .endloop2                     ; We are done filling remaining tiles with null data

    push af

    ld a, 15                            ; Fill 16 bytes of memory for each tile with 00s
    call memfill 

    pop af

    dec a                               ; Otherwise decrement the counter
    jp .loop2                           ; Loop again

.endloop2
    pop hl
    pop de
    ret
