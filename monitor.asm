; Sooooooo. New operating system for the rewritten architecture.

START:
    LOADI R0 0x136D8    	; Address of PLANE_CTRL
    LOADI R10 1         	; General increment
    STORE R0 R10        	; Enable plane 1
    LOADI R0 0x12840    	; We'll be using this as our cursor position from now on
    LOADI R9 3          	; Set our default palette. I chose palette 3, since the blue text is pretty.
    LOADI R10 1         	; we'll use this as our general increment register.
    LOADI R11 0x02860   	; this will be our input buffer.
    LOADI R12 0         	; this will be used to count how many addresses into the buffer we are.
    MOV   R14 R0        	; this will be used as a reference for our prompt's current position.
    LOADI R6 0x02840    	; we'll store a copy of the old keymap state at the very base of RAM.
    LOADI R7 0          	; We'll use this in a moment to wipe and initialize the keymap.
    LOADI R8 16         	; Length, in bytes, of our keyboard bitmap.

CLEAR_PREV:
    STORE R6 R7         	; Zeroes the byte address described in register 6.
    ADD   R6 R10        	; This just increments R6 by 1.
    SUB   R8 R10        	; Increments the count down by 1.
    JNZ   R8 CLEAR_PREV 	; Loop if register 8 ain't at 0 yet.
    LOADI R1 62         	; 62 is the ascii code for a greater-than sign, which we use as our prompt.
    WCHR  R0 R9 R1      	; Write it to the screen.
    ADD   R0 R10        	; Increment the cursor by 1.
    WCHR  R0 R9 R8      	; This is just a cheap way to get a blank character on screen.
				; R8 will always be 0 at this point, so we can use it in order to save an instr.
				; This would be very bad if it got into the input buffer, but here it's purely visual.
    ADD R0 R10   		; Anyways, increment the cursor by 1 and move on.

POLL_LOOP:
    LOADI R1 0x02804    	; Grab the base of the keymap state
    LOADI R6 0x2840     	; Grab the base of old keymap state
    LOADI R8 16         	; Reinitialize the 16 step counter.

CHECK_BYTES:
    LOAD R2 R1          	; Read the hardware keymap byte into register 2.
    LOAD R3 R6          	; And the state of the old keymap into register 3.
    MOV  R5 R2          	; Copy hardware keymap byte into register 5.
    XOR  R5 R3              	; Only if there's a difference between these values will R5 ever be greater than 0.
    JNZ  R5 KEY_CHANGED     	; That means we can check if it's greater than 0 to know if anything changed (key was pressed).
    ADD  R1 R10             	; Increment the hardware map cursor by 1,
    ADD  R6 R10             	; and the old keymap state cursor by 1,
    SUB  R8 R10             	; count down the step counter by 1,
    JNZ  R8 CHECK_BYTES     	; then take the next step, and loop over all that again, until we've taken 16 steps.
    JMP  POLL_LOOP          	; finally, after 16 paces in the very highly refined crystalline sand, move on.

KEY_CHANGED:
    MOV R5 R3        		; Grab a copy of XOR'd keymap
    NOT R5            		; then perform a bitwise invert on it
    AND R5 R2        		; and use this to bitmask it.
    JZ  R5 UPDATE_STATE     	; If R5 is 0, then this was just a key up operation.
    MOV R3 R10        		; If we got this far, though, it has to be a keydown, so let's make a bitmask.
    LOADI R4 0        		; This will be used as our bit index.

FIND_BIT:
    MOV R7 R5        		; Alright, copy the keymap into register 7, we're gonna do weird shit to it.
    AND R7 R3        		; Fuckin told you. If this is greater than zero, then we found it.
    JNZ R7 BIT_SET          	; Jump to the BIT_SET subroutine, we know where it lives now.
    SHL R3 R10        		; Otherwise, shift over by one bit,
    ADD R4 R10        		; increment our bit index,
    JMP FIND_BIT            	; then loop til we find the little fucker.

BIT_SET:
    LOADI R7 0x02804   		; Okay. Grab the base of the keymap and load it into R7.
    SUB   R1 R7        		; Find our offset into the keymap and shove it in R1,
    LOADI R7 8        		; then set R7's new value as a byte offset.
    MUL   R1 R7        		; Multiply our new offset by 8,
    ADD   R1 R4        		; and guess what, with that, we have our physical key ID.
    LOADI R7 37        		; Check for Enter (ID 37)
    SUB   R7 R1        		; Subtract offset from R7,
    JZ    R7 HANDLE_ENTER   	; and handle enter if the result is 0.
    LOADI R7 53        		; Check for a backspace next (ID 53),
    SUB   R7 R1        		; subtract the bit offset like before,
    JZ  R7 HANDLE_BACKSPACE 	; then handle backspace if the result is 0.
    LOADI R6 0x0280B    	; Lastly, grab byte 7 of the hardware keymap,
    LOAD  R7 R6        		; load its value into register 7,
    LOADI R8 0xC0        	; and grab the key IDs of our shift keys,
    AND   R7 R8        	 	; then compare them with a bitwise and.
    JNZ   R7 LOOKUP_SHIFTED 	; If the result is greater than 0, go fetch the shifted value which that key corresponds to.

LOOKUP_UNSHIFTED:
    LOADI R7 TABLE_U    	; Grab the base address of the unshifted table,
    ADD   R1 R7         	; add that address to our key ID to find target in the table,
    LOAD  R1 R1         	; snatch the value at that address,
    JZ    R1 UPDATE_STATE   	; If the value is 0, which is typical for all non-character-printing-keys, we can ignore it.
    JMP PRINT_CHAR          	; Otherwise? Print that bitch!

LOOKUP_SHIFTED:
    LOADI R7 TABLE_S        ; Grab the base address of the shifted table,
    ADD   R1 R7             ; add that address to our key ID to find target in the table,
    LOAD  R1 R1             ; snatch the value at that address,
    JZ    R1 UPDATE_STATE   ; If value is 0, as is typical for non-character-printing-keys, ignore.
    JMP PRINT_CHAR          ; Otherwise? Print that bastard (upgraded from bitch)!

PRINT_CHAR:
    WCHR  R0  R9 R1        	; Print the found character to the screen at our cursor position,
    STORE R11 R1        	; save to the RAM buffer,
    ADD   R11 R10        	; increment the buffer pointer,
    ADD   R12 R10        	; update buffer length,
    ADD   R0  R10        	; then increment the cursor,
    JMP UPDATE_STATE    	; and store the keymap state as the old keymap state, so we can do this all over again.

UPDATE_STATE:
    LOADI R1 0x02804    	; Point to the hardware keymap
    LOADI R6 0x02840    	; Point to the RAM keymap
    LOADI R8 16         	; Initiate a 16 step counter.

UPDATE_LOOP:
    LOAD  R2 R1         	; Read hardware state,
    STORE R6 R2         	; save it to this RAM address,
    ADD   R1 R10        	; increment our byte address in the hardware keymap,
    ADD   R6 R10        	; increment our byte address in the RAM keymap,
    SUB   R8 R10        	; decrement our counter,
    JNZ   R8 UPDATE_LOOP    	; and keep walking till we've taken sixteen steps.
    JMP POLL_LOOP        	; We've taken 16 paces in the sand, so let's start this duel all over again.

HANDLE_BACKSPACE:
    JZ R12 UPDATE_STATE    	; If buffer empty do nothing
    MOV R7 R0			; Copy the cursor real quick so we don't break it...
    SUB R7 R14			; Subtract from prompt start...
    JZ  R7 UPDATE_STATE		; We're at the prompt, so do nothing.
    SUB R12 R10            	; Decrement buffer length
    SUB R11 R10            	; Decrement buffer pointer
    SUB R0 R10             	; Move cursor back one cell
    LOADI R1 0             	; Blank character
    WCHR R0 R9 R1          	; Erase character on screen
    JMP UPDATE_STATE       	; Sync keymap state

HANDLE_ENTER:
    WCHR  R14 R9 R7        	; Since R7 is still set to 0 from the bit_set check if we got here,
    WCHR  R0  R9 R7        	; we can use it to write NULL characters over the old prompt,
    STORE R11 R7        	; and NULL terminate our string in RAM.
    CALL  NEWLINE		; Print new line to screen
    MOV   R14 R0		; Update cursor position
HANDLE_COMMAND:			; Then let's handle some commands.
    LOADI R15 0x02860    	; Grab the base of input buffer,
    CALL  SKIP_SPACE    	; skip the leading whitespace,
    LOAD  R1  R15       	; and load the byte at the address in the text buffer.
    JZ    R1  PRINT_PROMPT  	; Oh, is there nothing here? Uh, just print a new prompt i guess, that was enter-on-blank.

    ; Check for ECHO
    LOADI R13 CMD_ECHO
    CALL  STRCMP
    JNZ   R1 DO_ECHO

    ; Check for PAL
    LOADI R13 CMD_PAL
    CALL  STRCMP
    JNZ   R1 DO_PAL

    ; Unknown command
    JMP UNKNOWN_CMD

STRCMP:
    PUSH R15
    PUSH R13
STRCMP_LOOP:
    LOAD R2  R15        	; Load the character from user input
    LOAD R3  R13        	; Load the character from ROM string
    JZ   R3  STRCMP_MATCH    	; If the ROM string is 0, we hit the end of the command name.
    SUB  R2  R3        		; If we didn't do that, though, let's compare the characters.
    JNZ  R2  STRCMP_FAIL    	; If they differ, it's not a match...
    ADD  R15 R10        	; Increment the character from user input,
    ADD  R13 R10        	; then the character from ROM string,
    JMP STRCMP_LOOP        	; and loop over this again until null terminator or mismatch.
STRCMP_MATCH:              	; Was a match;
    POP   R13        		; Restore ROM pointer,
    POP   R7         		; Restore our return address,
    LOADI R1 1       		; then return 1.
    RET              		;
STRCMP_FAIL:         		; Wasn't a match;
    POP   R13        		; Restore ROM pointer,
    POP   R15        		; restore scan pointer,
    LOADI R1 0       		; and return 0.
    RET              		;
WRAP_TO_TOP:            	; Wrap to top by...
    LOADI R0 0x12840    	; Well. Yeah you can just set the cursor to the start of the character buffer I guess, but-
    CALL  NEWLINE    		; Okay then, I guess it really is that simple. Huh.

PRINT_PROMPT:
    MOV   R14 R0        	; Update R14 with new prompt position,
    LOADI R1  62        	; grab the ASCII ID for '>',
    WCHR  R0  R9 R1         	; then print it to the terminal;
    ADD   R0  R10          	; increment the cursor,
    SUB   R1  R1           	; zero the value of R1,
    WCHR  R0  R9 R1        	; then print that too,
    ADD   R0  R10        	; and increment the cursor once more.
    LOADI R11 0x02860    	; Reset buffer pointer to start of our input buffer,
    SUB   R12 R12        	; reset buffer length to 0,
    JMP UPDATE_STATE     	; then synchronize state and wait for input.

SKIP_SPACE:
    LOAD  R1 R15           	; Read current character from input buffer
    LOADI R2 32            	; ASCII id of space character
    SUB R1 R2              	; Subtract value of space from curr char
    JNZ R1 SKIP_SPACE_DONE 	; if result not 0, then not a space
    ADD R15 R10            	; increment cursor by 1
    JMP SKIP_SPACE         	; loop until not a space
SKIP_SPACE_DONE:
    RET

NEWLINE:
    LOADI R6 0x12840      	; Start of character buffer
    MOV R7 R0              	; Current cursor
    SUB R7 R6              	; Get byte offset into char buffer
    LOADI R8 60            	; Screen width
    DIV R7 R8              	; Offset by current row index
    ADD R7 R10             	; Next row index (curr + 1)
    MUL R7 R8              	; Next row index * screen width
    MOV R0 R7              	; Move cursor to resulting offset
    ADD R0 R6              	; Add base address of char buffer
    LOADI R7 0x12F47       	; Last address of character buffer
	LOADI R6 NEWLINE_WRAP	; Load label for our relational jump
    JGT R0 R7 R6 		; Past end of buffer? Wrap to top
    RET
NEWLINE_WRAP:
    LOADI R0 0x12840       	; Start of character buffer
    RET

; PAL command - change palette
DO_PAL:
    ADD   R9 R10        	; Increment palette
    LOADI R7 3          	; Bitmask
    AND   R9 R7         	; Mask to 2 bits
    JMP PRINT_PROMPT    	; New prompt

; ECHO command: Repeat user-supplied string
DO_ECHO:
    CALL SKIP_SPACE        	; Skip the space.

ECHO_LOOP:
    LOAD R1 R15            	; Read current character from input buffer
    JZ R1 ECHO_DONE        	; if character is 0 (null char) command end
    WCHR R0 R9 R1          	; else write curr char to the character bu>
    ADD R0 R10             	; Increment print cursor by 1
    ADD R15 R10            	; Increment scan cursor by 1
    JMP ECHO_LOOP          	; Loop until null char

ECHO_DONE:
    CALL NEWLINE
    JMP PRINT_PROMPT

; Unknown command error handling
UNKNOWN_CMD:
    LOADI R15 UNKNOWN_STR  	; Unknown error string
    CALL  ERR_PRINT        	; Print ERR
    JMP   PRINT_PROMPT     	; New prompt

ERR_PRINT:
    PUSH  R15              	; save the message pointer
    LOADI R15 ERR_STR      	; Load error string from eof
    CALL  PRINT_STR        	; print string
    POP   R15              	; restore message pointer
    CALL  PRINT_STR        	; print main error message
    CALL  NEWLINE          	; goto newline
    RET

; Important helpers for multiple commands and subroutines...
PRINT_STR:
    LOAD  R1 R15
    JZ    R1 PRINT_STR_DONE
    WCHR  R0 R9 R1
    ADD   R0 R10
    ADD   R15 R10
    JMP   PRINT_STR
PRINT_STR_DONE:
    RET

TABLE_U:
    .byte 97  ; a
    .byte 98  ; b
    .byte 99  ; c
    .byte 100 ; d
    .byte 101 ; e
    .byte 102 ; f
    .byte 103 ; g
    .byte 104 ; h
    .byte 105 ; i
    .byte 106 ; j
    .byte 107 ; k
    .byte 108 ; l
    .byte 109 ; m
    .byte 110 ; n
    .byte 111 ; o
    .byte 112 ; p
    .byte 113 ; q
    .byte 114 ; r
    .byte 115 ; s
    .byte 116 ; t
    .byte 117 ; u
    .byte 118 ; v
    .byte 119 ; w
    .byte 120 ; x
    .byte 121 ; y
    .byte 122 ; z
    .byte 49  ; 1
    .byte 50  ; 2
    .byte 51  ; 3
    .byte 52  ; 4
    .byte 53  ; 5
    .byte 54  ; 6
    .byte 55  ; 7
    .byte 56  ; 8
    .byte 57  ; 9
    .byte 48  ; 0
    .byte 32  ; space
    .byte 10  ; enter
    .byte 0   ; F1
    .byte 0   ; F2
    .byte 0   ; F3
    .byte 0   ; F4
    .byte 0   ; F5
    .byte 0   ; F6
    .byte 0   ; F7
    .byte 0   ; F8
    .byte 0   ; F9
    .byte 0   ; F10
    .byte 0   ; F11
    .byte 0   ; F12
    .byte 0   ; Escape
    .byte 0   ; Tab
    .byte 0   ; Caps Lock
    .byte 8   ; Backspace
    .byte 0   ; Home
    .byte 0   ; End
    .byte 0   ; PgUp
    .byte 0   ; PgDn
    .byte 0   ; Up
    .byte 0   ; Down
    .byte 0   ; Left
    .byte 0   ; Right
    .byte 0   ; LShift
    .byte 0   ; RShift
    .byte 0   ; LCtrl
    .byte 0   ; RCtrl
    .byte 0   ; LAlt
    .byte 0   ; RAlt
    .byte 0   ; padding
    .byte 0   ; padding
    .byte 0   ; padding
    .byte 0   ; padding
    .byte 96  ; Grave
    .byte 45  ; minus
    .byte 61  ; equals
    .byte 91  ; Square bracket left
    .byte 93  ; Square bracket Right
    .byte 92  ; Backslash
    .byte 59  ; Semicolon
    .byte 39  ; Apostrophe
    .byte 44  ; Comma
    .byte 46  ; Period
    .byte 47  ; Forslash

TABLE_S:
    .byte 65  ; A
    .byte 66  ; B
    .byte 67  ; C
    .byte 68  ; D
    .byte 69  ; E
    .byte 70  ; F
    .byte 71  ; G
    .byte 72  ; H
    .byte 73  ; I
    .byte 74  ; J
    .byte 75  ; K
    .byte 76  ; L
    .byte 77  ; M
    .byte 78  ; N
    .byte 79  ; O
    .byte 80  ; P
    .byte 81  ; Q
    .byte 82  ; R
    .byte 83  ; S
    .byte 84  ; T
    .byte 85  ; U
    .byte 86  ; V
    .byte 87  ; W
    .byte 88  ; X
    .byte 89  ; Y
    .byte 90  ; Z
    .byte 33  ; !
    .byte 64  ; @
    .byte 35  ; #
    .byte 36  ; $
    .byte 37  ; %
    .byte 94  ; ^
    .byte 38  ; &
    .byte 42  ; *
    .byte 40  ; Right parenthesis
    .byte 41  ; Left parenthesis
    .byte 32  ; Space
    .byte 10  ; Enter
    .byte 0   ; F1
    .byte 0   ; F2
    .byte 0   ; F3
    .byte 0   ; F4
    .byte 0   ; F5
    .byte 0   ; F6
    .byte 0   ; F7
    .byte 0   ; F8
    .byte 0   ; F9
    .byte 0   ; F10
    .byte 0   ; F11
    .byte 0   ; F12
    .byte 0   ; Escape
    .byte 0   ; Tab
    .byte 0   ; Caps Lock
    .byte 8   ; Backspace
    .byte 0   ; Home
    .byte 0   ; End
    .byte 0   ; PgUp
    .byte 0   ; PgDn
    .byte 0   ; Up
    .byte 0   ; Down
    .byte 0   ; Left
    .byte 0   ; Right
    .byte 0   ; LShift
    .byte 0   ; RShift
    .byte 0   ; LCtrl
    .byte 0   ; RCtrl
    .byte 0   ; LAlt
    .byte 0   ; RAlt
    .byte 0   ; padding
    .byte 0   ; padding
    .byte 0   ; padding
    .byte 0   ; padding
    .byte 126 ; Tilde
    .byte 95  ; Underscore
    .byte 43  ; Plus symbol
    .byte 123 ; Open  brace
    .byte 125 ; Close brace
    .byte 124 ; Vertical bar
    .byte 58  ; Colon
    .byte 34  ; Quotation marks
    .byte 60  ; Less-than
    .byte 62  ; Greater-than
    .byte 63  ; Question mark

; Command Strings
CMD_ECHO:
    .byte 69  ; 'E'
    .byte 67  ; 'C'
    .byte 72  ; 'H'
    .byte 79  ; 'O'
    .byte 0   ; Null terminator

CMD_FILE:
    .byte 70  ; 'F'
    .byte 73  ; 'I'
    .byte 76  ; 'L'
    .byte 69  ; 'E'
    .byte 0   ; Null terminator

CMD_FORMAT:
    .byte 70  ; 'F'
    .byte 79  ; 'O'
    .byte 82  ; 'R'
    .byte 77  ; 'M'
    .byte 65  ; 'A'
    .byte 84  ; 'T'
    .byte 0   ; Null terminator

CMD_LOAD:
    .byte 76
    .byte 79
    .byte 65
    .byte 68
    .byte 0

UNKNOWN_STR:
    .byte 85  ; U
    .byte 110 ; n
    .byte 107 ; k
    .byte 110 ; n
    .byte 111 ; o
    .byte 119 ; w
    .byte 110 ; n
    .byte 32  ;
    .byte 99  ; c
    .byte 111 ; o
    .byte 109 ; m
    .byte 109 ; m
    .byte 97  ; a
    .byte 110 ; n
    .byte 100 ; d
    .byte 33  ; !
    .byte 0

ERR_STR:
    .byte 69
    .byte 82
    .byte 82
    .byte 58
    .byte 32
    .byte 0

CMD_RUN:
    .byte 82  ; R
    .byte 85  ; U
    .byte 78  ; N
    .byte 0

CMD_PAL:
    .byte 80
    .byte 65
    .byte 76
    .byte 0

BADADDR_STR:
    .byte 66  ; B
    .byte 97  ; a
    .byte 100 ; d
    .byte 32  ;
    .byte 97  ; a
    .byte 100 ; d
    .byte 100 ; d
    .byte 114 ; r
    .byte 101 ; e
    .byte 115 ; s
    .byte 115 ; s
    .byte 33  ; !
    .byte 0
