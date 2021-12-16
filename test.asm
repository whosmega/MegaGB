SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    

EntryPoint:
    ld hl, $2000
    ld [hl], $06
    ld hl, $4000
    ld [hl], $02
    stop
    
