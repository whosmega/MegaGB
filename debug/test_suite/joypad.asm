INCLUDE "macros.inc"

SECTION "JoypadVector", rom0[$60]
    call joypadInterruptHandler
    reti

SECTION "Header", rom0[$100]
    jp init
    ds $150 - @, 0                  ; make room for the header    

init:
    ld hl, string1
    call printStringNL
    
    ld hl, $FFFF                    ; load IE register address
    set 4, [hl]                     ; enable joypad interrupt
    ei                              ; enable interrupts
    jp end

; ------------------------------------------------------
joypadInterruptHandler:
    push hl
    push af

    ld hl, $FF00                    ; load joypad register address
    set 5, [hl]                     ; keep only direction buttons enabled
    bit 3, [hl]
    jp z, .S
    bit 2, [hl]
    jp z, .W
    bit 1, [hl]
    jp z, .A
    bit 0, [hl]
    jp z, .D
    jp .directionEnd

.W:
    ld hl, string3
    jp .directionPrint
.A:
    ld hl, string4
    jp .directionPrint
.S:
    ld hl, string5
    jp .directionPrint
.D:
    ld hl, string6
    jp .directionPrint

.directionPrint:
    call printStringNL

.directionEnd:

    pop af
    pop hl
    ret

end:
    halt
    jp end

string1:
    db "Press WASD", 0

string2:
    db "You used the joypad!", 0

string3:
    db "You pressed W", 0

string4:
    db "You pressed A", 0

string5:
    db "You pressed S", 0

string6:
    db "You pressed D", 0
