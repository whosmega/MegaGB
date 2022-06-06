SECTION "Header", rom0[$100]
    jp main
    ds $150 - @, 0


main:
    call waitVblank
    /* Disable ppu */
    ld hl, $FF40
    res 7, [hl]
    
    call initTiles
    call initPalette
    call initTileMap

    ld hl, $FF40
    /* Enable ppu */
    set 7, [hl]
    jp done

initTiles:
    push bc
    /* Initialise tile data we need */
    /* Load red tile at index 0 */
    ld hl, $8000
    ld bc, tile1
    call loadTile

    /* Load blue tile at index 1 */
    inc hl
    ld bc, tile2
    call loadTile

    pop bc
    ret

/* ------------------------------------- */
loadTile:
    /* Tile address in hl 
       Tile data pointer in bc */
    push hl
    push bc
    push af

    ld a, 16
.loop:
    cp a, 0
    jp z, .loopEnd

    push af
    push hl
    push bc
    pop hl
    ld a, [hl]
    pop hl
    ld [hli], a
    pop af

    inc bc
    dec a
    jp .loop

.loopEnd
    pop af
    pop bc
    pop hl
    ret
/* -------------------------------------- */ 
initPalette:
    push hl
    push bc 

    ld hl, $FF68
    ld a, $C0
    ld [hl], a
    
    ld hl, $FF69
    ld a, 16

.loop1:
    cp a, 0
    jp Z, .loop1End
    ld b, $00
    ld c, $1F

    ld [hl], c
    ld [hl], b
    
    ld b, $7C
    ld c, $00

    ld [hl], c
    ld [hl], b
    dec a
    jp .loop1
   
.loop1End
    pop bc
    pop hl
    ret

initTileMap:
    push hl
    push bc
    push af
    push de

    ld hl, $9800
    ld c, 9
    ld b, 0
    ld e, 0

/* Load full tilemap with the same tile */
.loop1
    ld a, c
    cp 0
    jp z, .loop1End
    call loadTileRow
    push de
    ld de, 64
    add hl, de
    pop de
    dec c
    jp .loop1
    
.loop1End:
    ld hl, $9800 + 32
    ld c, 9
    ld b, 1
    ld a, e
    cp 1
    jp z, .end
    ld e, 1
    jp .loop1

.end
    pop de
    pop af
    pop bc
    pop hl
    ret

loadTileRow:
    /* Tile row address in HL
     * Data to load in B */
    push bc
    push hl
    push af
    ld c, 20
.loop:
    ld a, c 
    cp 0
    jp z, .loopEnd
    ld a, b
    ld [hli], a
    dec c
    jp .loop

.loopEnd
    pop af
    pop hl
    pop bc
    ret 

waitVblank:
    ld a, [$FF44]
    cp a, 144
    jp nz, waitVblank
    ret

done:
    jp done


tile1:
    ds 16, 00
tile2:
    ds 16, $ff, 00
