# Helio210
A ~1:1 logical emulator for my homebrew fantasy computer, the Helio210. Based on the proprietary Helio 2410 CPU and the Flare 2 GPU, with integrated emulators for the General Instrument AY-3-8910 Programmable Sound Generator and dual R6551 ACIA UART Controllers. A basic operating system and some demo programs are included. 
# Specsheet
```
------------------------------------------------------

     Helio 210 Memory Map & System Specifications

------------------------------------------------------

0000 0000 0000 0000 0000 - Start of System ROM
0000 0010 0000 0000 0000 - Start of Stack  RAM
0000 0010 1000 0000 0000 - Start of Random Number
0000 0010 1000 0000 0100 - Start of Keymap
0000 0010 1000 0001 0100 - Start of System Clock
0000 0010 1000 0001 1000 - Start of System Control
0000 0010 1000 0001 1010 - Start of System Audio
0000 0010 1000 0010 1010 - Start of System Status
0000 0010 1000 0011 0010 - Start of Serial Port
0000 0010 1000 0011 1010 - Start of UART Control
0000 0010 1000 0100 0000 - Start of Primary SRAM
0001 0010 1000 0100 0000 - Start of Character Buffer 1
0001 0010 1111 0100 1000 - Start of Palette Buffer 1
0001 0010 1111 1000 1000 - Start of Character Buffer 2
0001 0011 0110 1001 0000 - Start of Palette Buffer 2
0001 0011 0110 1101 1000 - Start of Graphics Control
0001 0011 0110 1110 0000 - Start of Font Buffer
0001 0101 0110 1110 0000 - Start of PMEM 1
0001 1101 0110 1110 0000 - Start of PMEM 2
0010 0101 0110 1110 0000 - Start of EXTPMEM
0011 0101 0110 1101 1111 - Last address of EXTPMEM

0x00000 -> 0x01FFF : System ROM
0x02000 -> 0x027FF : Stack  RAM
0x02800 -> 0x02803 : Random Number
0x02804 -> 0x02813 : Keymap hah
0x02814 -> 0x02817 : System Clock
0x02818 -> 0x02819 : System Control
0x0281A -> 0x02829 : System Audio
0x0282A -> 0x02831 : System Status
0x02832 -> 0x02839 : Serial Port
0x0283A -> 0x0283F : UART Control
0x02840 -> 0x1283F : Primary SRAM
0x12840 -> 0x12F47 : Character Buffer 1
0x12F48 -> 0x12F87 : Palette Buffer 1
0x12F88 -> 0x1368F : Character Buffer 2
0x13690 -> 0x136D7 : Palette Buffer 2
0x136D8 -> 0x136DF : Graphics Control
0x136E0 -> 0x156DF : Font Buffer
0x156E0 -> 0x1D6DF : PMEM 1
0x1D6E0 -> 0x256DF : PMEM 2
0x256E0 -> 0x356DF : EXTPMEM haha

Stack RAM is word-addressed, and can
only be written to or read from
with PUSH, POP, CALL and RET.

Character buffers are word-addressed,
and can only be written to or read
with WCHR and RCHR.

All other RAM is byte-addressed.

------------------------------------------------------

          Helio 2410 Processor Specifications

------------------------------------------------------

CPU Architecture for the Helio 2410:
  Sixteen 32-bit internal scratch registers
  One dedicated 12-bit register for SP
  One dedicated 24-bit register for PC
  24-bit memory address bus
  16-bit data bus
  10MHz clock speed

Instruction set...
  All instructions whose first byte starts with 0x1 through 0x5
  are two-word opcodes. All others are one-word opcodes.
  PC is a 24-bit byte address. PC is incremented by 4 for
  two-word instructions, and is otherwise incremented by 2.
  PC points at the first byte of the current instruction.

  Most memory is byte-addressable. A 16-bit access at byte
  address N reads bytes N and N+1 and assembles them into
  one 16-bit word; N may be odd or even. Instruction fetch
  is a 16-bit access at PC, so PC may be odd or even. The
  CPU does not mask, fault, or otherwise constrain PC bit 0.
  A 16-bit access at an odd address may occupy additional
  bus cycles depending on how memory is implemented; this
  affects timing only, not instruction encoding or
  architectural state.

  Byte order within a 16-bit word: at byte address N,
  byte N holds the high 8 bits and byte N+1 holds the
  low 8 bits. This rule applies to instruction words,
  LOADI immediates, and SRAM cells alike.

  The stack and character buffers must be 16-bit-word
  addressable, and can only be reached or written to
  using the dedicated instructions PUSH, POP, CALL, RET,
  WCHR and RCHR; these regions are protected, and any
  READ or WRITE to them through any other instruction
  is considered an *ILLEGAL REGION ACCESS.

  Technically, every address from the CPU's perspective
  is a cell-address, but in all practical terms this
  resolves to every region being either word-addressed
  (16-bit-wide cells) or byte-addressed (8-bit-wide-cells).

  * Strangely, due to the design of this CPU, a stack
    overflow or stack underflow is recoverable; ILLEGAL
    REGION ACCESS, on the other hand, raises a bus error,
    and forces a total reset of the CPU. standard RAM is
    not cleared on a CPU reset barring total power loss,
    but all registers besides the SP are zeroed and the
    cpu reenters memory at address 0x000000. SP value is
    preserved across this reset because stack RAM is not
    cleared, and so reinitializing the SP value would
    orphan live stack data.

Main codes:
  0x0 SUBCODE                 (Prefix for subcode operations)
  0x1 LOADI   Ra, imm24       (Load 24-bit immediate into Ra)
  0x2 JMP     addr24          (Jump to addr24)
  0x3 JZ      Ra, addr24      (Jump to addr24 if Ra == 0)
  0x4 JNZ     Ra, addr24      (Jump to addr24 if Ra != 0)
  0x5 CALL    addr24          (Push PC, then jump to addr24)
  0x6 RET                     (Pop PC from stack)
  0x7 SHL     Ra, shamt       (Shift Left;  shamt = imm6 or register)
  0x8 SHR     Ra, shamt       (Shift Right; shamt = imm6 or register)
  0x9 JLT     Ra, Rb, Rc      (Jump to address in Rc if Ra < Rb)
  0xA JGT     Ra, Rb, Rc      (Jump to address in Rc if Ra > Rb)
  0xB RND     Ra, imm8        (Random Number)
  0xC PUSH    Ra              (Push Ra onto stack)
  0xD POP     Ra              (Pop from stack into Ra)
  0xE WCHR    Ra, Rb, Rc      (Write character Rc to address Ra using palette Rb)
  0xF RCHR    Ra, Rb, Rc      (Read character at Ra into palette Rb and char Rc)
Sub codes:
  0x0 NOP                     (No Operation)
  0x1 HALT                    (Halt the CPU)
  0x2 LOAD    Ra, Rb          (Load memory at address Rb into Ra)
  0x3 STORE   Ra, Rb          (Store Rb into memory at address Ra)
  0x4 ADD     Ra, Rb          (Add       Rb to   Ra)
  0x5 MUL     Ra, Rb          (Multiply  Ra by   Rb)
  0x6 SUB     Ra, Rb          (Subtract  Rb from Ra)
  0x7 DIV     Ra, Rb          (Divide    Ra by   Rb)
  0x8 MOD     Ra, Rb          (Modulo/Remainder)
  0x9 AND     Ra, Rb          (Bitwise AND)
  0xA OR      Ra, Rb          (Bitwise OR)
  0xB XOR     Ra, Rb          (Bitwise XOR)
  0xC NOT     Ra              (Bitwise Invert)
  0xD MOV     Ra, Rb          (Copy value from one register to another)
  0xE MOVPC   Ra              (Move PC into register)
  0xF JMPR    Ra              (Jump to address in Ra)
  0xF CALLR   Ra              (Push PC, jump to address in Ra)

Subcodes are encoded as the low nibble
of the first byte; the remaining 8 bits
of the instruction word are interpreted
as the subcode's operands. Main codes
use the low nibble of the first byte as
well as the remaining 8 bits of the word
as their operands.

JMPR and CALLR share the sub-opcode 0xF.
To differentiate between them, the CPU
reads the low nibble of the second byte.
The assembler reads JMPR & CALLR and
translates, inserting the value for
proper operation into this field.

PUSH splits register value in half, sending
low 16 bits first, then high 16 across the
data bus. POP reads the high 16 bits first,
then the low 16 bits, and reconstructs the
full register value with an OR gate.

CALL splits the PC address into two words,
pushing the low 16 bits first, then high 8
zero-extended to 16 across the data bus.

RET pops the high 8 bits first, then low 16,
reconstructs as PC = (high8 << 16) | low 16

The stack pointer (SP) is a dedicated
12-bit register holding a cell index
into 16-bit-word-addressable RAM with
SP ranging over 0x000–0xFFF. Stack offset
from 0x00000 as well as stack size are
set with a hardware switch at install.
On reset, the CPU reads the configured
stack size and sets SP as size minus 1.
The stack grows downward toward 0x000.
This behavior is very simple, but it
unfortunately means that address 0x000
itself is not usable, and the effective
stack size is in reality 2047 words.

If SP would leave STACK region on a PUSH
or POP, the CPU raises STACK OVERFLOW or
STACK UNDERFLOW and leaves SP unchanged,
and no memory is written.

LOADI, JMP, JZ, JNZ and CALL encode imm24
as one word and one byte, high 8 first,
then low 16.

Indirect jumps target low 24 bits of
target register's value.

MUL preserves low 32 bits of result,
all overflow is silently lost.
MOD by 0 results in Ra == 0.

DIV / 0 results in Ra == 0xFFFFFFFF,
software must check Rb for zero if
the distinction matters.

MOD / 0 results in Ra == 0x00000000,
software must check Rb for zero if
the distinction matters.

Divide by zero sentinels are designed
to output useful constants rather than
arithmetic results. DIV / 0 sets all
bits in target register high, MOD / 0
sets all bits in target register low.

SHR and SHL interpret the bit 7 of the
operand field as a flag. If low, performs
a logical shift, interpreting last 6 bits
as an immediate. If high, interprets the
last 4 bits as a register ID, and uses the
value of the target register as the shift
count. Any shift count > 31 sets Ra's value
to 0. Any bits not used for operation are
ignored in both modes of operation.

WCHR & RCHR assume character & palette data
are each at most 8 bits long, and outputs
them simultaneously. RCHR zero-extends to
fill target registers, while WCHR only
reads the bottom 8 bits of Rb & Rc.

Every arithmetic and comparison instruction
interprets its operands as unsigned 32-bit
integers. Signed integer interpretation is
not native to the ISA, and thus it must be
implemented in software if needed.

------------------------------------------------------

    Helio FLARE Graphical Processor Specifications

------------------------------------------------------

The Helio 210 uses the proprietary Helio Flare 2
graphical processing unit to handle all its graphical
needs. It is text-mode only, but supports two planes
of text with transparency on the upper plane.
For colors, it uses 4 palettes per text plane, each
with 16 colors, and each character can be printed in
any of the 4 palettes on a per-character basis. The
first and second plane have separate palette buffers
and must be individualized independently.

The Flare 2 is a separate chip with its own read-only
memory port. Scanout reads from the character buffers,
palette buffers, font buffer, and Graphics Control
block are serviced independently of the CPU. CPU
accesses to 0x12840-0x156DF are serviced through a
separate read/write interface; because the GPU only
reads, scanout never contends with CPU writes; CPU
reads may be arbitrated against scanout reads.

WCHR and RCHR are the only defined CPU paths into the
character buffers; LOAD and STORE reach the palette
buffers, font buffer, and Graphics Control block.
The GPU itself only has one externally-visible latch
register that it can write to, labeled as STATUS in
the memory map. All other externally-visible latch
registers are read-only from the GPU's perspective
and only written to by the CPU; the only things
besides STATUS that the GPU ever writes to are
its own internal scratch registers.

The 210 motherboard also includes a Motorola MC1377 
RGB-to-composite encoder. The encoder takes the 
Flare 2's RGB and composite sync outputs and 
produces a standard NTSC or PAL composite video 
signal for connection to a television or composite 
monitor. The encoder's PAL/NTSC mode is selected by 
the same hardware region switch that configures the 
Flare 2's scanout timing.

Graphics control memory map:
  ----------------------------------------------------------------
  Addr      Name         R/W  Description
  ----------------------------------------------------------------

  0x136D8   PLANE_CTRL   R/W  bit 0 = plane 1 enable (1 = render),
                              bit 1 = plane 2 enable (1 = render).
                              All other bits reserved, write 0.

  0x136D9   CURSOR_X     R/W  Hardware cursor column, 0–59.
                              Values ≥ 60 are off-screen right.

  0x136DA   CURSOR_Y     R/W  Hardware cursor row, 0–29.
                              Values ≥ 30 are off-screen below.

  0x136DB   CURSOR_CTRL  R/W  bit 0 = cursor enable
                              bit 1 = cursor blink enable
                              bit 2 = cursor plane select (
                              0 = plane 1, 1 = plane 2    )
                              bits 3–7 reserved.

  0x136DC   SCROLL_X     R/W  Horizontal scroll offset in pixels
                              (0-7). Wraps the 480-pixel line.

  0x136DD   SCROLL_Y     R/W  Vertical scroll offset in pixels,
                              0–7. Wraps the 240-pixel field.

  0x136DE   BORDER       R/W  Border/overscan color, RRRGGGBB.

  0x136DF   STATUS       R    bit 0 = vblank (1 during vertical
                              blanking), bit 1 = hblank (1 during
                              horizontal blanking), bit 2 = frame(
                              toggles once per frame; software   |
                              XORs successive reads to detect a  |
                              new frame.                         )
                              bit 3 = region ( 0 = PAL, 1 = NTSC )
                              bits 4–7 read 0.

Character cells:
  Each character-buffer address holds one 16-bit cell.

  Bits 0–7 are the character code (0–255), indexing the Font Buffer.

  Bits 8–15 are the attribute byte: bits 0–1 select one of four
  palettes for this cell; bits 2–7 are reserved.

Font Buffer:
  256 glyphs of 8×8 pixels at 4 bits per pixel, 32 bytes per glyph,
  packed two pixels per byte (high nibble = left pixel). A pixel
  value of 0–15 is an index into the cell's selected palette.

Palettes:
  Four palettes of 16 colors each, stored as RRRGGGBB bytes.
  Each cell selects its palette via the attribute byte;
  each glyph pixel's 4-bit value indexes into that palette.

Transparency:
  Palette Buffer 2 is 72 bytes - 64 bytes of color data
  followed by an 8-byte (64-bit) transparency bitmap.

  Bit 0 of byte 0 corresponds to palette 0, color 0;
  the bitmap runs color-major within each palette,
  palette 0 through 3.

  A set bit marks that palette entry transparent;
  a clear bit marks it opaque.

  During plane-2 scanout, a pixel whose palette entry
  is transparent does not overwrite the plane-1 pixel
  beneath it.

  Palette buffer 1 is 64 bytes; four palettes of 16 colors.
  It has no transparency bitmap and always renders opaque.

Cursor:
  The hardware cursor is a scanout-time transform and does not
  modify memory. When CURSOR_CTRL bit 0 is set, the GPU XORs
  the 4-bit pixel value of each pixel in the cell at (CURSOR_X,
  CURSOR_Y) with 0xF before palette lookup, on the plane
  selected by CURSOR_CTRL bit 2.

  Cursor pixels bypass plane-2 transparency testing and are
  always opaque, so the cursor remains visible regardless of
  the cell's palette or transparency configuration.

  When CURSOR_CTRL bit 1 (blink) is set, the cursor toggles on
  and off every 16 frames (approximately 0.32 s on and 0.32 s
  off in PAL mode; 0.27 s in NTSC mode). When bit 1 is clear,
  the cursor is always on.

  CURSOR_X values ≥ 60 or CURSOR_Y values ≥ 30
  disable the cursor.

Screen:
  60 columns × 30 rows of 8×8 glyphs = 480×240 pixels.

Region switch:
  A hardware switch selects PAL or NTSC sync timing.

  The switch does not change active resolution, cell format,
  palette format, or memory map; it changes only line length,
  line count, blanking, and refresh rate.

  The switch position is reported in STATUS bit 3 (0 = PAL, 1 = NTSC).
  The Flare 2 outputs RGB; no composite color encoding is performed,
  and the PAL and NTSC labels describe sync timing only.

Scanout timing:
  The Flare 2 emits 640 pixels per scanline in PAL mode and
  636 in NTSC mode, of which 480 are active (60 cells × 8 pixels)
  in both. A frame is 312 lines in PAL mode and 262 in NTSC mode,
  of which 240 are active (30 rows × 8 pixels) in both. The pixel
  clock is 10 MHz in both modes.

  PAL mode:
    640 pixels per line (480 active; 160 blanking: 16 front porch,
    48 sync, 96 back porch); 312 lines per frame (240 active; 72
    blanking including vertical sync); line rate 15.625 kHz; frame
    rate 50.08 Hz.

  NTSC mode:
    636 pixels per line (480 active; 156 blanking: 16 front porch,
    48 sync, 92 back porch); 262 lines per frame (240 active; 22
    blanking including vertical sync); line rate 15.723 kHz;
    frame rate 60.01 Hz.

  The 240 active lines are displayed in both modes; the remaining 72
  (PAL) or 22 (NTSC) lines are filled with BORDER color. Because the
  active area is identical in both modes, software need not adapt
  layout to region.

  STATUS bit 1 (hblank) is set during
  the blanking pixels of each scanline.

  STATUS bit 0 (vblank) is set during
  the blanking lines of each frame.

  STATUS bit 2 (frame) toggles at
  the start of each vblank.

------------------------------------------------------

    General Instrument AY-3-8910 PSG Specifications

------------------------------------------------------

The Helio 210 uses the General Instrument AY-3-8910
Programmable Sound Generator (PSG) to generate audio.
The AY-3-8910 is a 3-channel square wave and noise
generator with a shared hardware envelope controller.

Synthesis Channels:
  The chip provides three tone channels (A, B, C).

  Each channel's frequency is set by a 12-bit
  divider value (split across two 8-bit registers,
  fine and coarse tune). The 12-bit value divides
  the input clock to produce the output tone.

  The chip also includes a single noise generator
  with a 5-bit period control, and a single envelope
  generator shared across all three channels.

Mixer & Output:
  Register 7 controls the mixer. Each channel can be
  configured to output pure tone, pure noise, or a
  mixture of both. The mixer bits are active-low
  (0 enables the source). Volume per channel is
  controlled by a 4-bit value (0-15). Bit 4 of
  each volume register enables hardware envelope
  modulation, which overrides the fixed 4-bit volume.

Envelope Generator:
  The envelope controller has a 16-bit period
  (fine and coarse registers) and a 4-bit shape
  register controlling attack, alternate, continue,
  and hold functions. It is a simple ramp-based
  generator, not a full ADSR, and is shared
  by all three channels.

Host Interface & Clocking:
  The AY-3-8910 exposes a multiplexed 8-bit address/
  data bus with a 3-bit bus control interface
  (BDIR, BC1, BC2) for register selection, reads,
  and writes. The chip is clocked at 1.25 MHz,
  derived from the 10 MHz CPU clock by a divide-by-8.

  Internally, the tone and noise generators are
  clocked at Φ / 16, and the envelope generator
  at Φ / 256, meaning register writes only take
  effect on the next internal update tick.

  With Φ = 1.25 MHz, the tone and noise tick is
  78.125 kHz, and the envelope tick is 4.8828 kHz.
  Register writes take effect on the next internal
  update tick (maximum 12.8 µs latency at 78.125 kHz),
  which is negligible at audio rates.

  Tone frequency for a 12-bit period TP is 78,125 /
  TP Hz (≈19.08 Hz at TP = 4095 to 78.125 kHz at TP = 1).
  Noise rate for a 5-bit period NP is 78,125 / NP Hz.
  Envelope period for a 16-bit period EP is 256 × EP /
  1.25 MHz seconds.

  The AY-3-8910 provides two 8-bit general-purpose
  I/O ports (Port A and Port B), though their use
  in the Helio 210 is not yet defined.

Register Map (16 registers, selected via address latch):
  Register    Function
  R0/R1       Channel A tone fine/coarse (12-bit divider)
  R2/R3       Channel B tone fine/coarse
  R4/R5       Channel C tone fine/coarse
  R6          Noise period (5-bit)
  R7          Mixer control (tone/noise enable per channel, active-low)
  R8/R9/R10   Channel A/B/C volume (4-bit + envelope enable)
  R11/R12     Envelope period fine/coarse (16-bit)
  R13         Envelope shape
  R14/R15     I/O Port A / Port B data

------------------------------------------------------

          Rockwell R6551 ACIA Specifications

------------------------------------------------------

The Helio 210 uses two Rockwell R6551 Asynchronous
Communications Interface Adapters (ACIA) for serial I/O.
The 6551 is a 6500/6800-family peripheral that converts
parallel data from the CPU into asynchronous serial NRZ,
LSB first, and back again. Each chip includes an on-chip
baud rate generator driven by a single external crystal,
so no separate divider is required.

Channel Assignment:
  UART 0 is the network/host port.
  UART 1 is the printer port.

  The designation is a software convention;
  the two ACIAs are identical and either
  may be used for either purpose.

Host Interface:
  Each 6551 exposes four internal registers selected by
  two register-select lines (RS1, RS0), which on the
  Helio 210 are driven by address bits A1 and A0 of its
  4-byte block. Address bit A2 selects between the ACIAs.

RS1 RS0 Read (R/W high) Write (R/W low)

0 0 Receiver Data Transmitter Data
0 1 Status Register Programmed Reset
1 0 Command Register Command Register
1 1 Control Register Control Register

Only the Command and Control registers
are readable and writable. The Programmed
Reset operation does not transfer data;
it clears bits 4–0 of the Command register
and bit 2 of the Status register.

The register set below is identical for both ACIAs;
"its block" refers to the four bytes at 0x02832 or 0x02836.

Status Register (read at RS1:RS0 = 01):
  Bit 7 IRQ Interrupt has occurred (cleared on Status read)
  Bit 6 DSR Data Set Ready (0 = ready, 1 = not ready)
  Bit 5 DCD Data Carrier Detect (0 = detected, 1 = not detected)
  Bit 4 TDRE Transmitter Data Register Empty (1 = empty)
  Bit 3 RDRF Receiver Data Register Full (1 = full)
  Bit 2 OVRN Overrun (1 = overrun has occurred)
  Bit 1 FE Framing Error (1 = framing error)
  Bit 0 PE Parity Error (1 = parity error)

Bits 2–0 are self-clearing on a read of the Receiver Data
Register and do not generate interrupts.

Control Register (RS1:RS0 = 11):
  Bit 7 selects 1 or 2 stop bits.
  Bits 6–5 select word length (8, 7, 6, or 5 bits).
  Bit 4 selects the receiver clock source (
    0 = external 1× clock                 |
    1 = internal baud rate                )
    Bits 3–0 select the baud rate from the internal generator.

Baud Rate:
  Both ACIAS share a 1.8432 MHz crystal on UART 0,
  and XTLI of UART 1 is driven from the same
  oscillator with its XTLO left open.

  The Control register's low nibble selects one of 16 rates:
    0000 = 115,200 (external 16× clock) 1000 = 1,200
    0001 = 50 1001 = 1,800
    0010 = 75 1010 = 2,400
    0011 = 109.92 1011 = 3,600
    0100 = 134.58 1100 = 4,800
    0101 = 150 1101 = 7,200
    0110 = 300 1110 = 9,600
    0111 = 600 1111 = 19,200

All rates assume the 1.8432 MHz crystal.
The two channels are independently programmable;
the printer port will typically be set to 300–1200
baud and the network port to 9600.

Command Register (RS1:RS0 = 10):
  Bit 0 controls DTR              (
    0 = DTR high / receiver       |
    and all interrupts disabled,  |
    1 = DTR low / enabled         )

  Bit 1 disables the receiver interrupt.
  Bits 2–3 control the transmitter interrupt and RTS.
  Bit 4 enables receiver echo mode (requires bits 2–3 = 0).
  Bits 5–6 set the RTS output state.

  Because the Helio 2410 has no interrupt input,
  the interrupt-control bits are functionally irrelevant;
  the Command register is used to assert DTR and RTS for
  flow control, or may be written once at initialisation
  and left alone.

Clocking and Polled Operation:
  The 6551's register transfers are clocked by the
  peripheral bus φ2 at 1.25 MHz, derived from the
  10 MHz CPU clock by a divide-by-8 (the same divider
  that drives the AY-3-8910). This is within the R6551's
  φ2 limit. Because the CPU runs at 10 MHz and the
  peripheral bus at 1.25 MHz, each CPU access to a 6551
  register occupies eight CPU cycles on the bus; a polled
  driver should account for this when estimating throughput.
  The serial side of each ACIA runs at its own baud rate,
  independent of φ2, since the baud generator is driven by
  the shared 1.8432 MHz crystal.

The Helio 2410 has no interrupt system.
Serial I/O is therefore entirely polled:
software reads the Status register and
tests bit 3 (RDRF) before reading a
received byte, and tests bit 4 (TDRE)
before writing a byte to the Transmit
Data Register. No interrupt line from
the 6551 is connected.

Register Mapping on the Helio 210:

  UART 0 — network/host
  0x02832 : Receiver Data / Transmitter Data
  0x02833 : Status (read) / Programmed Reset (write)
  0x02834 : Command Register
  0x02835 : Control Register

  UART 1 — printer
  0x02836 : Receiver Data / Transmitter Data
  0x02837 : Status (read) / Programmed Reset (write)
  0x02838 : Command Register
  0x02839 : Control Register

  0x0283A -> 0x0283F : UART Control (reserved)

All framing, flow control, and higher-level protocol are
software responsibilities; the ACIA provides only byte-level
asynchronous serialisation and the status bits above.


____________________________________________________________________
|                                                                  |
|  _____ _   _ _____             _   _ _____ _     ___ ___  ____   |
| |_   _| | | | ____|           | | | | ____| |   |_ _/ _ \/ ___|  |
|   | | | |_| |  _|             | |_| |  _| | |    | | | | \___ \  |
|   | | |  _  | |___            |  _  | |___| |___ | | |_| |___) | |
|   |_| |_| |_|_____|           |_| |_|_____|_____|___\___/|____/  |
|                                                                  |
|   ____ ___  ____  ____   ___  ____      _  _____ ___ ___  _   _  |
|  / ___/ _ \|  _ \|  _ \ / _ \|  _ \    / \|_   _|_ _/ _ \| \ | | |
| | |  | | | | |_) | |_) | | | | |_) |  / _ \ | |  | | | | |  \| | |
| | |__| |_| |  _ <|  __/| |_| |  _ <  / ___ \| |  | | |_| | |\  | |
|  \____\___/|_| \_\_|    \___/|_| \_\/_/   \_\_| |___\___/|_| \_| |
|__________________________________________________________________|
|                    \                       /                     |
|                     \                     /                      |
|  \                   \ __________________/                    /  |
|   \        ___________/                  \___________        /   |
|    \  ____/                                          \____  /    |
|_    \/                                                    \/    _|
|                                                                  |
|                 The Center of the New Universe                   |
|__________________________________________________________________|
```
