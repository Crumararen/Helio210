; chime.asm - Plays a C Major arpeggio chime (Fixed Timing)
START:
    LOADI R1 1          ; Constant for the delay loop

    ; 1. Configure the Mixer (Enable Channel A Tone, Disable Noise)
    LOADI R2 0x02821    ; R7 (Mixer)
    LOADI R3 0x38       ; 0011 1000 (Active Low: 0=enable. Enables Tone A, B, C)
    STORE R2 R3

    ; 2. Set Channel A Volume to Max
    LOADI R2 0x02822    ; R8 (Channel A Volume)
    LOADI R3 0x0F       ; Max volume (15)
    STORE R2 R3

    ; 3. Play C5
    LOADI R2 0x0281A    ; R0 (Tone Fine)
    LOADI R3 0x95       ; Low 8 bits of 149
    STORE R2 R3
    LOADI R2 0x0281B    ; R1 (Tone Coarse)
    LOADI R3 0x00       ; High 4 bits of 149
    STORE R2 R3
    CALL DELAY

    ; 4. Play E5
    LOADI R2 0x0281A
    LOADI R3 0x76       ; Low 8 bits of 118
    STORE R2 R3
    CALL DELAY

    ; 5. Play G5
    LOADI R2 0x0281A
    LOADI R3 0x63       ; Low 8 bits of 99
    STORE R2 R3
    CALL DELAY

    ; 6. Play C6
    LOADI R2 0x0281A
    LOADI R3 0x4A       ; Low 8 bits of 74
    STORE R2 R3
    CALL ENDELAY

    ; 7. Silence the channel so it doesn't ring forever
    LOADI R2 0x02822
    LOADI R3 0x00
    STORE R2 R3

    HALT

DELAY:
    LOADI R4 1500       ; Outer loop (2000 * 2000 * 2 = 8,000,000 cycles = ~0.8 seconds)
DELAY_OUTER:
    LOADI R5 1000       ; Inner loop
DELAY_INNER:
    SUB R5 R1           
    JNZ R5 DELAY_INNER
    SUB R4 R1
    JNZ R4 DELAY_OUTER
    RET
ENDELAY:
    LOADI R4 3000
ENDELAY_OUTER:
    LOADI R5 2000
ENDELAY_INNER: 
    SUB R5 R1           
    JNZ R5 ENDELAY_INNER
    SUB R4 R1
    JNZ R4 ENDELAY_OUTER
    RET