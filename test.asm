SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    

EntryPoint:
    ld BC, 41
    ld A, [BC]
    ld HL, 19
    ld BC, 28
    add HL, BC
    daa
    stop
