SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    


EntryPoint:
    ld a, $00
    sbc a, $01
    stop



