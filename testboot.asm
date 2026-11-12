; testboot.asm - test for video and audio output;
; crawls across the screen buffer, writing random characters,
; writes "GOOD MORNING, WORLD!" to the screen, plays a chime, then wipes.
START:
    LOADI R0 0x136D8    ; Address of PLANE_CTRL
    LOADI R10 1         ; General increment
    STORE R0 R10        ; Enable plane 1
    LOADI  R0 0x12840   ; Cursor register starts at top left of character buffer 1
    LOADI  R1 0         ; Default glyph
    LOADI  R9 3         ; Palette 3
    LOADI R11 0x12F47   ; Last cell of character buffer 1
    LOADI R15 96        ; Glyph offset
    ; Configure the Mixer (Enable Channel A Tone, Disable Noise)
    LOADI R2 0x02821    ; R7 (Mixer)
    LOADI R3 0x38       ; 0011 1000 (Active Low: 0=enable)
    STORE R2 R3
    LOADI R2 0x02822    ; R8 (Channel A Volume)
    LOADI R3 0x03       ; Low volume (3)
    STORE R2 R3
    ; For first loop, play C4 (261 Hz, TP = 299 / 0x12B)
    LOADI R2 0x0281A    ; R0 (Tone Fine)
    LOADI R3 0x2B       ; Low 8 bits of 0x12B
    STORE R2 R3
    LOADI R2 0x0281B    ; R1 (Tone Coarse)
    LOADI R3 0x01       ; High 4 bits of 0x12B
    STORE R2 R3

LOOP:
    LOADI R2 RESET      ; Load JGT target
    JGT R0 R11 R2       ; if r0 is past end of character buffer 1, move to RESET
    RND R1 3            ; 3 bits of RNG
    ADD R1 R15          ; Apply glyph offset
    WCHR R0 R9 R1
    ADD R0, R10
    CALL DELAY
    JMP LOOP

RESET:
    JZ R9 END           ; if palette already 0, go to END
    LOADI R0 0x12840    ; otherwise, reset cursor to top left
    SUB R9 R10          ; increment palette down by 1
    MOV R8 R9           ; copy palette to R8
    JMP MUSINC          ; increment sound tone

MUSINC:

    ; If last palette, play G4 (392 Hz, TP = 199 / 0x0C7)
    LOADI R2 0x0281A
    LOADI R3 0xED       ; Low 8 bits of 0x0ED
    STORE R2 R3
    LOADI R2 0x0281B
    LOADI R3 0x00
    STORE R2 R3
    SUB R8 R10          ; Decrement palette
    JZ R8 LOOP          ; Go back to loop
    ; If palette 2, play E4 (329 Hz, TP = 237 / 0x0ED)
    LOADI R2 0x0281A
    LOADI R3 0xC7       ; Low 8 bits of 0x0C7
    STORE R2 R3
    SUB R8 R10          ; Decrement palette
    JZ R8 LOOP          ; Go back to loop
    ; If palette 1, play C5 (523 Hz, TP = 149 / 0x095)
    LOADI R2 0x0281A
    LOADI R3 0x95       ; Low 8 bits of 0x095
    STORE R2 R3
    SUB R8 R10          ; Decrement palette
    JMP LOOP            ; Go back to loop

END:
    LOADI R3 0x12840    ; Character buffer 1 base
    MOV   R0 R11        ; Copy end of character buffer to cursor
    SUB   R0 R3         ; R0 = Buffer end - Buffer base
    SHR   R0 R10        ; Cheaply divide R11 by 2
    LOADI R5 41         ; Load offset of half a column and half our planned last print
    SUB   R0 R5         ; Move pointer back 42 cells to center on a line
    ADD   R0 R3         ; Build an absolute address in the cursor
    LOADI R1 71         ; 'G'
    LOADI R2 79         ; 'O'
    LOADI R3 79         ; 'O'
    LOADI R4 68         ; 'D'
    LOADI R5 32         ; space
    LOADI R8  3         ; palette 3
    LOADI R9  2         ; palette 2
    WCHR R0 R9 R1
    ADD R0 R10
    WCHR R0 R8 R2
    ADD R0 R10
    WCHR R0 R9 R3
    ADD R0 R10
    WCHR R0 R8 R4
    ADD R0 R10
    WCHR R0 R8 R5
    ADD R0 R10
    LOADI R1 77         ; 'M'
    LOADI R2 79         ; 'O'
    LOADI R3 82         ; 'R'
    LOADI R4 78         ; 'N'
    LOADI R5 73         ; 'I'
    WCHR R0 R9 R1
    ADD R0 R10
    WCHR R0 R8 R2
    ADD R0 R10
    WCHR R0 R8 R3
    ADD R0 R10
    WCHR R0 R9 R4
    ADD R0 R10
    WCHR R0 R8 R5
    ADD R0 R10
    LOADI R1 78         ; 'N'
    LOADI R2 73         ; 'I'
    LOADI R3 78         ; 'N'
    LOADI R4 71         ; 'G'
    LOADI R5 44         ; comma
    LOADI R6 32         ; space
    WCHR R0 R9 R1
    ADD R0 R10
    WCHR R0 R8 R2
    ADD R0 R10
    WCHR R0 R9 R3
    ADD R0 R10
    WCHR R0 R8 R4
    ADD R0 R10
    WCHR R0 R9 R5
    ADD R0 R10
    WCHR R0 R8 R6
    ADD R0 R10
    LOADI R1 87         ; 'W'
    LOADI R2 79         ; 'O'
    LOADI R3 82         ; 'R'
    LOADI R4 76         ; 'L'
    LOADI R5 68         ; 'D'
    LOADI R6 33         ; exclamation point
    WCHR R0 R8 R1
    ADD R0 R10
    WCHR R0 R9 R2
    ADD R0 R10
    WCHR R0 R8 R3
    ADD R0 R10
    WCHR R0 R8 R4
    ADD R0 R10
    WCHR R0 R9 R5
    ADD R0 R10
    WCHR R0 R8 R6
    LOADI R0 0x12840    ; Return cursor to top left of screeen
    CALL MUSIC
    LOADI R1 0          ; null glyph (blank)
ENDLOOP:
    LOADI R2 DONE
    JGT R0 R11 R2
    WCHR R0 R9 R1
    ADD R0 R10
    CALL DELAY
    JMP ENDLOOP
DONE:
    ; Silence the channel
    LOADI R2 0x02822
    LOADI R3 0x00
    STORE R2 R3
    LOADI R0 0x12840
    HALT

DELAY:
    LOADI R12 3200
DELAY_LOOP:
    SUB R12 R10
    JNZ R12 DELAY_LOOP
    RET

MUSIC:
    LOADI R1 1          ; Constant for the delay loop

    ; 1. Set Channel A Volume to Max
    LOADI R2 0x02822    ; R8 (Channel A Volume)
    LOADI R3 0x0F       ; Max volume (15)
    STORE R2 R3

    ; 2. Play C4 (261 Hz, TP = 299 / 0x12B)
    LOADI R2 0x0281A    ; R0 (Tone Fine)
    LOADI R3 0x2B       ; Low 8 bits of 0x12B
    STORE R2 R3
    LOADI R2 0x0281B    ; R1 (Tone Coarse)
    LOADI R3 0x01       ; High 4 bits of 0x12B
    STORE R2 R3
    CALL MDELAY

    ; 3. Play E4 (329 Hz, TP = 237 / 0x0ED)
    LOADI R2 0x0281A
    LOADI R3 0xED       ; Low 8 bits of 0x0ED
    STORE R2 R3
    LOADI R2 0x0281B
    LOADI R3 0x00       ; High 4 bits are 0
    STORE R2 R3
    CALL MDELAY

    ; 4. Play G4 (392 Hz, TP = 199 / 0x0C7)
    LOADI R2 0x0281A
    LOADI R3 0xC7       ; Low 8 bits of 0x0C7
    STORE R2 R3
    LOADI R2 0x0281B
    LOADI R3 0x00
    STORE R2 R3
    CALL MDELAY

    ; 5. Play C5 (523 Hz, TP = 149 / 0x095)
    LOADI R2 0x0281A
    LOADI R3 0x95       ; Low 8 bits of 0x095
    STORE R2 R3
    LOADI R2 0x0281B
    LOADI R3 0x00
    STORE R2 R3
    CALL ENDELAY

    RET

MDELAY:
    LOADI R4 1500       ; Outer loop (2000 * 2000 * 2 = 8,000,000 cycles = ~0.8 seconds)
MDELAY_OUTER:
    LOADI R5 1000       ; Inner loop
MDELAY_INNER:
    SUB R5 R1           
    JNZ R5 MDELAY_INNER
    SUB R4 R1
    JNZ R4 MDELAY_OUTER
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
