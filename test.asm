SECTION "Header", rom0[$100]
    jp EntryPoint
    ds $150 - @, 0                  ; make room for the header    

EntryPoint:
    ld B, 32
    ld C, 230
    ld D, 100
    ld E, 30
    call AddSub
    stop

AddSub:
    push DE
    push BC
    push AF
    ld A, B
    add A, C
    ld B, A
    ld A, D
    sub A, E
    ld D, A
    ld A, B
    add A, D
    pop AF
    pop BC
    pop DE
    ret
