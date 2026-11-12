; rndcrawl.asm - crawls across the screen buffer, writing random characters,
; writes DONE to the screen, wipes, then loops infinitely.
START:
    LOADI R0 0x136D8    ; Address of PLANE_CTRL
    LOADI R10 1         ; General increment
    STORE R0 R10        ; Enable plane 1         
    LOADI  R0 0x12840   ; Cursor register starts at top left of character buffer 1
    LOADI  R1 0         ; Default glyph
    LOADI  R9 3         ; Palette 3
    LOADI R11 0x12F47   ; Last cell of character buffer 1
    LOADI R15 96        ; Glyph offset

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
    JMP LOOP

END:
    LOADI R3 0x12840    ; Character buffer 1 base
    MOV   R0 R11        ; Copy end of character buffer to cursor
    SUB   R0 R3         ; R0 = Buffer end - Buffer base
    SHR   R0 R10        ; Cheaply divide R11 by 2
    LOADI R5 32         ; Load offset of half a column and half our planned last print
    SUB   R0 R5         ; Move pointer back 32 cells to center on a line
    ADD   R0 R3         ; Build an absolute address in the cursor
    LOADI R1 68         ; 'D'
    LOADI R2 79         ; 'O'
    LOADI R3 78         ; 'N'
    LOADI R4 69         ; 'E'
    LOADI R9  2         ; palette 2
    WCHR R0 R9 R1       ; write 'D' at cursor
    ADD R0 R10          ; increment    cursor
    WCHR R0 R9 R2       ; write 'O' at cursor
    ADD R0 R10          ; increment    cursor
    WCHR R0 R9 R3       ; write 'N' at cursor
    ADD R0 R10          ; increment    cursor
    WCHR R0 R9 R4       ; write 'E' at cursor
    ADD R0 R10          ; increment    cursor
    LOADI R0 0x12840    ; Return cursor to top left of screeen
    LOADI R1 0          ; null glyph (blank)
ENDLOOP:
    LOADI R2 DONE
    JGT R0 R11 R2
    WCHR R0 R9 R1
    ADD R0 R10
    CALL DELAY
    JMP ENDLOOP
DONE:
    LOADI R0 0x12840
    JMP START

DELAY:
    LOADI R12 3200
DELAY_LOOP:
    SUB R12 R10
    JNZ R12 DELAY_LOOP
    RET