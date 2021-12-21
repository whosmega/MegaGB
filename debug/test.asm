SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    

EntryPoint:
    ld a, $1
    scf
    adc $f
    stop
    
