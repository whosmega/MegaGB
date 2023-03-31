SECTION "Header", rom0[$100]
    nop
    jp main
    ds $150 - @, 0

main:
