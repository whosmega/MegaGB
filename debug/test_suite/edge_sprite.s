SECTION "Header", rom0[$100]
    nop
    jp main
    ds $150 - @, 0

INCLUDE "interface.inc"

main:
    
    call initScreen
    jp end

end:
    di
    halt
    jp end

initScreen:
    push hl
    /* Disables screen, loads tiles, sets up tilemap */  

    call pollVBlank

    ld hl, R_LCDC
    res 7, [hl]                     ; Disable PPU

    call initPalettes

    ld bc, tileData
    ld a, 4 - 1
    call loadTiles_8000h 
    set 7, [hl]                     ; Enable PPU

    pop hl
    ret

SECTION "ReadOnly", rom0

tileData:
.black:         ds 16, $FF
.darkgray:      ds 16, $AA
.lightgray:     ds 16, $55
.white:         ds 16, $00

s1: db "lmao", 0
