SECTION "Header", rom0[$100]
    jp init
    ds $150 - @, 0

init:
    call waitVblank
    call done
    call waitVblankEnd
    call waitVblank
    call done
    call waitVblankEnd
    call waitVblank
    call done
    call waitVblankEnd
    call waitVblank
    call done
    ld b, b

waitVblank:
    ld a, [$FF44]
    cp a, 144
    jp nz, waitVblank
    ret

waitVblankEnd:
    ld a, [$FF44]
    cp a, 0
    jp nz, waitVblankEnd
    ret

done:
    ld hl, $9800
    ld [hl], 0
    ret

