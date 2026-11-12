// helio210.c - Emulator for the Helio 210 fantasy computer
// Uses SDL for window management and keyboard input handling
// Build: gcc -O2 -o helio210 helio210.c uart.c AY3-8910.c AY3-8910_sdl.c $(sdl2-config --cflags --libs) 
// or     gcc -O2 -o helio210 helio210.c uart-stub.c AY3-8910.c AY3-8910_sdl.c $(sdl2-config --cflags --libs) 
// ...if you do not want to compile with the external uart interface code.
// Usage: ./helio213 - Boots the emulator and loads persistent memory files
//                     into the RAM, simulates the behavior of the CPU and
//                     original memory map as faithfully as possible.
//                     Computer enters at address 0x00000 and runs until
//                     HALT or ERROR. Exit window with ESCAPE key to end.

#include "uart.h"
#include "AY3-8910.h"
#include "AY3-8910_sdl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Persistent memory file definitions
#define ROM_FILE     "memory/rom.bin"
#define PMEM1_FILE   "memory/pmem1.bin"
#define PMEM2_FILE   "memory/pmem2.bin"
#define EXTPMEM_FILE "memory/extpmem.bin"
#define DEFAULT_FONT "memory/font.bin"
#define SAVE_INTERNAL_MS 1000

// Main memory map      Address
#define ROM_BASE        0x00000
#define ROM_END         0x01FFF
#define STACK_BASE      0x02000
#define STACK_END       0x027FF
#define RND_REGISTER    0x02800
#define KEYMAP_BASE     0x02804
#define SYSTEM_CLOCK    0x02814
#define SYSCNTRL_BASE   0x02818
#define SYSCNTRL_END    0x02819
#define AUDIO_BASE      0x0281A
#define AUDIO_END       0x02829
#define STATUS_BASE     0x0282A
#define STATUS_END      0x02831
#define SERIAL_BASE     0x02832
#define SERIAL_END      0x02839
#define UARTCNTRL_BASE  0x0283A
#define UARTCNTRL_END   0x0283F
#define RAM_BASE        0x02840
#define RAM_END         0x1283F
#define CHARBUFF1_BASE  0x12840
#define CHARBUFF1_END   0x12F47
#define PALBUFF1_BASE   0x12F48
#define PALBUFF1_END    0x12F87
#define CHARBUFF2_BASE  0x12F88
#define CHARBUFF2_END   0x1368F
#define PALBUFF2_BASE   0x13690
#define PALBUFF2_END    0x136D7
#define GPUCNTRL_BASE   0x136D8
#define GPUCNTRL_END    0x136DF
#define FONTBUFF_BASE   0x136E0
#define FONTBUFF_END    0x156DF
#define PMEM1_BASE      0x156E0
#define PMEM1_END       0x1D6DF
#define PMEM2_BASE      0x1D6E0
#define PMEM2_END       0x256DF
#define EXTPMEM_BASE    0x256E0
#define EXTPMEM_END     0x356DF

// Screen details
#define SCR_COLS 60
#define SCR_ROWS 30
#define CHAR_W   8
#define CHAR_H   8
#define UPSCALE  3
#define WIN_W    (SCR_COLS * CHAR_W * UPSCALE)  // Grab the width of all the characters and multiply by the upscale
#define WIN_H    WIN_W * 3 / 4                  // Then derive the height from that, assuming 4:3 aspect ratio

// Helper defines
#define STACK_SIZE ((STACK_END - STACK_BASE) + 1)

// Actual memory variables
static uint8_t          rom[ROM_END       - ROM_BASE       + 1];
static uint16_t       stram[STACK_END     - STACK_BASE     + 1];
static uint8_t     syscntrl[SYSCNTRL_END  - SYSCNTRL_BASE  + 1];
static uint8_t       status[STATUS_END    - STATUS_BASE    + 1];
static uint8_t  uartcontrol[UARTCNTRL_END - UARTCNTRL_BASE + 1];
static uint8_t          ram[RAM_END       - RAM_BASE       + 1];
static uint16_t charbuffer1[CHARBUFF1_END - CHARBUFF1_BASE + 1];
static uint8_t   palbuffer1[PALBUFF1_END  - PALBUFF1_BASE  + 1];
static uint16_t charbuffer2[CHARBUFF2_END - CHARBUFF2_BASE + 1];
static uint8_t   palbuffer2[PALBUFF2_END  - PALBUFF2_BASE  + 1];
static uint8_t   gpucontrol[GPUCNTRL_END  - GPUCNTRL_BASE  + 1];
static uint8_t   fontbuffer[FONTBUFF_END  - FONTBUFF_BASE  + 1];
static uint8_t        pmem1[PMEM1_END     - PMEM1_BASE     + 1];
static uint8_t        pmem2[PMEM2_END     - PMEM2_BASE     + 1];
static uint8_t      extpmem[EXTPMEM_END   - EXTPMEM_BASE   + 1];

// Hardware memory variables
static uint32_t random_cell = 0;        // Random number - 32-bit RNG cell
static uint8_t  keymap[16] = {0};       // Keymap        - 128 bits for keyboard matrix
static uint32_t last_clock_tick = 0;    // Last clock tick for calculating uptime
static uint32_t system_clock = 0;       // Second        - Centisecond counter\

// CPU structure, variable type, helper functions for later
typedef struct {
    uint32_t r[16];                     // Sixteen 32-bit internal scratch registers
    uint16_t sp;                        // Simulate SP with 16 bit var - mask below
    uint32_t pc;                        // Simulate PC with 32 bit var - mask below
    bool halted;                        // flags true if CPU is halted, freezes emulator
    bool fatal;                         // fatal error; flags true if CPU_ERROR, resets cpu
    bool error;                         // non-fatal error boolean.
    const char *errmsg;
} CPU;

static inline void set_sp(CPU *c, uint32_t v) { c->sp = v & 0x0FFF; }
static inline void set_pc(CPU *c, uint32_t v) { c->pc = v & 0xFFFFFF; }
static inline uint16_t get_sp(const CPU *c)   { return c->sp; }
static inline uint32_t get_pc(const CPU *c)   { return c->pc; }

// Used to initialize the CPU, ensuring all 0 values
static void init_cpu(CPU *c) {
    memset(c->r, 0, sizeof(c->r));
    set_sp(c, (STACK_SIZE - 1));
    set_pc(c, ROM_BASE);
    c->halted = false;
    c->fatal  = false;
    c->error  = false;
    c->errmsg = NULL;
}

// On a bus error or illegal region access, cpu must be able to be reset
static void reset_cpu(CPU *c) {         // in a way that preserves the
    memset(c->r, 0, sizeof(c->r));      // stack pointer, because the
    set_pc(c, ROM_BASE);                // stack is not zeroed in such
    c->halted = false;                  // a case. Technically, this
    c->fatal  = false;                  // could be used as a program
    c->errmsg = NULL;                   // exit command by intentionally
                                        // performing an illegal region 
}                                       // access, but if you do that, i hate you.

static CPU cpu;

// Memory access
static uint32_t mem_read(uint32_t addr) {
    const char *CHRBUFERR = "READ from character buffer";                           // Kept fucking up my pretty comments. smh. get var'd
    if   (addr <= ROM_END)             return   rom[addr];                          // System ROM
    if   (addr <= STACK_END) {cpu.fatal=1;cpu.errmsg="READ from STACK";return 0;}   // Stack  RAM
    if   (addr >= RND_REGISTER && addr <= RND_REGISTER + 3) {                       // Random Number
        return (random_cell  >> ((addr - RND_REGISTER) * 8)) & 0xFF;                //
    } if (addr >= SYSTEM_CLOCK && addr <= SYSTEM_CLOCK + 3) {                       // System Clock
        return (system_clock >> ((addr - SYSTEM_CLOCK) * 8)) & 0xFF;                //
    } if (addr >= KEYMAP_BASE && addr <= KEYMAP_BASE + 15) {                        // Keymap
        return keymap[addr - KEYMAP_BASE];                                          //
    } if (addr <= SYSCNTRL_END)  return    syscntrl[addr - SYSCNTRL_BASE];          // System Control
    if (addr >= AUDIO_BASE && addr <= AUDIO_END)return psg_read(addr - AUDIO_BASE); // System Audio
    if   (addr <= STATUS_END)    return      status[addr - STATUS_BASE];            // System Status
        if (addr >= SERIAL_BASE && addr <= SERIAL_END) {                            // Serial port
        int unit = (addr >= SERIAL_BASE + 4) ? 1 : 0;                               // 
        int reg  = (addr - SERIAL_BASE) & 3;                                        // 
        return acia_read(unit, reg);                                                // 
    } if (addr <= UARTCNTRL_END) return uartcontrol[addr - UARTCNTRL_BASE];         // UART Control
    if   (addr <= RAM_END)       return         ram[addr - RAM_BASE];               // Primary SRAM
    if   (addr <= CHARBUFF1_END) {cpu.fatal=1;cpu.errmsg=CHRBUFERR;return 0;}       // Character Buffer 1
    if   (addr <= PALBUFF1_END)  return  palbuffer1[addr - PALBUFF1_BASE];          // Palette   Buffer 1
    if   (addr <= CHARBUFF2_END) {cpu.fatal=1;cpu.errmsg=CHRBUFERR;return 0;}       // Character Buffer 2
    if   (addr <= PALBUFF2_END)  return  palbuffer2[addr - PALBUFF2_BASE];          // Palette   Buffer 2
    if   (addr <= GPUCNTRL_END)  return  gpucontrol[addr - GPUCNTRL_BASE];          // Graphics Control
    if   (addr <= FONTBUFF_END)  return  fontbuffer[addr - FONTBUFF_BASE];          // Font Buffer
    if   (addr <= PMEM1_END)     return       pmem1[addr - PMEM1_BASE];             // PMEM 1
    if   (addr <= PMEM2_END)     return       pmem2[addr - PMEM2_BASE];             // PMEM 2
    if   (addr <= EXTPMEM_END)   return     extpmem[addr - EXTPMEM_BASE];           // EXTPMEM
    cpu.fatal=1; cpu.errmsg="READ out of range"; return 0;                          // ERROR: READ OUT OF RANGE!GE!
}

static void mem_write (uint32_t addr, uint32_t val) {
    if (addr <= ROM_END)   { cpu.fatal=1;cpu.errmsg="WRITE to ROM";   return; }     // System ROM
    if (addr <= STACK_END) { cpu.fatal=1;cpu.errmsg="WRITE to STACK"; return; }     // Stack  RAM
    if (addr >= RND_REGISTER && addr <= RND_REGISTER + 3) {                         // Random Number
        uint32_t shift = (addr - RND_REGISTER) * 8;                                 //   Get cell address
        random_cell = (random_cell & ~(0xFF << shift)) | ((val & 0xFF) << shift);   //   Clear target byte, shift input to target byte,
        return;                                                                     //   and write to the zeroed target byte using OR.
    } if (addr >= SYSTEM_CLOCK && addr <= SYSTEM_CLOCK + 3) {                       // System Clock
        uint32_t shift = (addr - SYSTEM_CLOCK) * 8;                                 //   Get cell address
        system_clock = (system_clock & ~(0xFF << shift)) | ((val & 0xFF) << shift); //   Clear target byte, shift input to target byte,
        return;                                                                     //   and write to the zeroed target byte using OR.
    } if (addr >= KEYMAP_BASE && addr <= KEYMAP_BASE + 15) {                        // Keymap
        keymap[addr - KEYMAP_BASE] = val & 0xFF; return;                            //   Write bottom byte of val to target address
    } if (addr >= SYSCNTRL_BASE && addr <= SYSCNTRL_END) {                          // System Control
        syscntrl[addr - SYSCNTRL_BASE] = val & 0xFF; return;                        //   Works just like the keymap
    } if (addr >= AUDIO_BASE && addr <= AUDIO_END) {                                // System Audio
        psg_write(addr - AUDIO_BASE, val & 0xFF); return; }                         //   Works much like the serial port
    // I hate how wide this is cuz it fucks up all my pretty comments but this is the most readable least aggrivating working version I can make
    if   (addr <= STATUS_END)    {      status[addr - STATUS_BASE   ] = (uint8_t) val; return; }  // System Status
    if   (addr <= SERIAL_END) {int unit = (addr >= SERIAL_BASE + 4) ? 1 : 0;                      // Serial port
         int reg  = (addr - SERIAL_BASE) & 3;acia_write(unit, reg, (uint8_t)val);      return; }  // Still the serial port
    if   (addr <= UARTCNTRL_END) { uartcontrol[addr - UARTCNTRL_BASE] = (uint8_t) val; return; }  // UART Control
    if   (addr <= RAM_END)       {         ram[addr - RAM_BASE      ] = (uint8_t) val; return; }  // Primary SRAM
    if   (addr <= CHARBUFF1_END) { cpu.fatal=1;cpu.errmsg="WRITE to character buffer"; return; }  // Character Buffer 1
    if   (addr <= PALBUFF1_END)  {  palbuffer1[addr - PALBUFF1_BASE ] = (uint8_t) val; return; }  // Palette   Buffer 1
    if   (addr <= CHARBUFF2_END) { cpu.fatal=1;cpu.errmsg="WRITE to character buffer"; return; }  // Character Buffer 2
    if   (addr <= PALBUFF2_END)  {  palbuffer2[addr - PALBUFF2_BASE ] = (uint8_t) val; return; }  // Palette   Buffer 2
    if   (addr <= GPUCNTRL_END)  {  gpucontrol[addr - GPUCNTRL_BASE ] = (uint8_t) val; return; }  // Graphics Control
    if   (addr <= FONTBUFF_END)  {  fontbuffer[addr - FONTBUFF_BASE ] = (uint8_t) val; return; }  // Font Buffer
    if   (addr <= PMEM1_END)     {       pmem1[addr - PMEM1_BASE    ] = (uint8_t) val; return; }  // PMEM 1
    if   (addr <= PMEM2_END)     {       pmem2[addr - PMEM2_BASE    ] = (uint8_t) val; return; }  // PMEM 2
    if   (addr <= EXTPMEM_END)   {     extpmem[addr - EXTPMEM_BASE  ] = (uint8_t) val; return; }  // EXTPMEM
    // Thank god, it's over. Anyways if you got this far you fucked up... Throw an error, you went to far, passed go, collected $200, all that, smh
    cpu.fatal=1; cpu.errmsg="WRITE out of range"; return;                          // ERROR: WRITE OUT OF RANGE!
}

static void write_char (uint32_t addr, uint16_t val) {
    if (addr >= CHARBUFF1_BASE && addr <= CHARBUFF1_END) { charbuffer1[addr - CHARBUFF1_BASE] = (uint16_t) val; return; }
    if (addr >= CHARBUFF2_BASE && addr <= CHARBUFF2_END) { charbuffer2[addr - CHARBUFF2_BASE] = (uint16_t) val; return; }
    cpu.fatal=1; cpu.errmsg="WCHR outside character buffer"; return;
}

static uint16_t read_char(uint32_t addr) {
    if (addr >= CHARBUFF1_BASE && addr <= CHARBUFF1_END) return charbuffer1[addr - CHARBUFF1_BASE];
    if (addr >= CHARBUFF2_BASE && addr <= CHARBUFF2_END) return charbuffer2[addr - CHARBUFF2_BASE];
    cpu.fatal=1; cpu.errmsg="RCHR outside character buffer"; return 0;
}

static void push_to_stack (uint32_t val) {
    if (cpu.sp == 0) {cpu.error=1; cpu.errmsg="STACK OVERFLOW"; return; }
    stram[cpu.sp] = (uint16_t) val; cpu.sp -= 1; return;
}

static uint16_t pop_from_stack () {
    if (cpu.sp >= STACK_SIZE - 1) {cpu.error=1; cpu.errmsg="STACK UNDERFLOW"; return 0; }
    cpu.sp += 1; uint16_t poppage = stram[cpu.sp];
    stram[cpu.sp] = stram[cpu.sp] & 0x0000;
    return poppage;
}

static uint32_t build_24 (uint32_t v1, uint32_t v2) {
    return ( ( v1 & 0xFF ) << 16 | ( v2 & 0xFFFF ) );
}

static uint32_t build_16 (uint32_t v1, uint32_t v2) {
    return ( ( v1 & 0xFF ) << 8 | ( v2 & 0xFF ) );
}

static uint32_t build_8 (uint32_t v1, uint32_t v2) {
    return ( ( v1 & 0xFF ) << 4 | ( v2 & 0xF ) );
}

// Keyboard map
static const struct { 
    SDL_Keycode sym; 
    int byte_idx; 
    int bit_idx; 
} key_mappings[] = {
    // Byte 0: A-H
    { SDLK_a, 0, 0 }, { SDLK_b, 0, 1 }, { SDLK_c, 0, 2 }, { SDLK_d, 0, 3 },
    { SDLK_e, 0, 4 }, { SDLK_f, 0, 5 }, { SDLK_g, 0, 6 }, { SDLK_h, 0, 7 },
    // Byte 1: I-P
    { SDLK_i, 1, 0 }, { SDLK_j, 1, 1 }, { SDLK_k, 1, 2 }, { SDLK_l, 1, 3 },
    { SDLK_m, 1, 4 }, { SDLK_n, 1, 5 }, { SDLK_o, 1, 6 }, { SDLK_p, 1, 7 },
    // Byte 2: Q-X
    { SDLK_q, 2, 0 }, { SDLK_r, 2, 1 }, { SDLK_s, 2, 2 }, { SDLK_t, 2, 3 },
    { SDLK_u, 2, 4 }, { SDLK_v, 2, 5 }, { SDLK_w, 2, 6 }, { SDLK_x, 2, 7 },
    // Byte 3: Y, Z, 1-6
    { SDLK_y, 3, 0 }, { SDLK_z, 3, 1 }, 
    { SDLK_1, 3, 2 }, { SDLK_2, 3, 3 }, { SDLK_3, 3, 4 }, { SDLK_4, 3, 5 }, 
    { SDLK_5, 3, 6 }, { SDLK_6, 3, 7 },
    // Byte 4: 7-0, Space, Enter, F1, F2
    { SDLK_7, 4, 0 }, { SDLK_8, 4, 1 }, { SDLK_9, 4, 2 }, { SDLK_0, 4, 3 },
    { SDLK_SPACE, 4, 4 }, { SDLK_RETURN, 4, 5 }, { SDLK_F1, 4, 6 }, { SDLK_F2, 4, 7 },
    // Byte 5: F3-F10
    { SDLK_F3, 5, 0 }, { SDLK_F4, 5, 1 }, { SDLK_F5, 5, 2 }, { SDLK_F6, 5, 3 },
    { SDLK_F7, 5, 4 }, { SDLK_F8, 5, 5 }, { SDLK_F9, 5, 6 }, { SDLK_F10, 5, 7 },
    // Byte 6: F11, F12, Esc, Tab, Caps, Backspace, Home, End
    { SDLK_F11, 6, 0 }, { SDLK_F12, 6, 1 }, { SDLK_ESCAPE, 6, 2 }, { SDLK_TAB, 6, 3 },
    { SDLK_CAPSLOCK, 6, 4 }, { SDLK_BACKSPACE, 6, 5 }, { SDLK_HOME, 6, 6 }, { SDLK_END, 6, 7 },
    // Byte 7: PgUp, PgDn, Arrows, Shifts
    { SDLK_PAGEUP, 7, 0 }, { SDLK_PAGEDOWN, 7, 1 }, { SDLK_UP, 7, 2 }, { SDLK_DOWN, 7, 3 },
    { SDLK_LEFT, 7, 4 }, { SDLK_RIGHT, 7, 5 }, { SDLK_LSHIFT, 7, 6 }, { SDLK_RSHIFT, 7, 7 },
    // Byte 8: finish modifiers + GUI/Menu
    { SDLK_LCTRL, 8, 0 }, { SDLK_RCTRL, 8, 1 }, { SDLK_LALT, 8, 2 }, { SDLK_RALT, 8, 3 },
    { SDLK_LGUI, 8, 4 }, { SDLK_RGUI, 8, 5 }, { SDLK_APPLICATION, 8, 6 },
    // (byte 8 bit 7 = ID 71 reserved)
    // Byte 9: top-row punctuation (unshifted / shifted must be handled by drivers)
    { SDLK_BACKQUOTE, 9, 0 },      // ` ~
    { SDLK_MINUS, 9, 1 },          // - _
    { SDLK_EQUALS, 9, 2 },         // = +
    { SDLK_LEFTBRACKET, 9, 3 },    // [ {
    { SDLK_RIGHTBRACKET, 9, 4 },   // ] }
    { SDLK_BACKSLASH, 9, 5 },      // \ |
    { SDLK_SEMICOLON, 9, 6 },      // ; :
    { SDLK_QUOTE, 9, 7 },          // ' "

    // Byte 10: bottom-row punctuation + misc nav
    { SDLK_COMMA, 10, 0 },         // , <
    { SDLK_PERIOD, 10, 1 },        // . >
    { SDLK_SLASH, 10, 2 },         // / ?
    { SDLK_INSERT, 10, 3 },
    { SDLK_DELETE, 10, 4 },
    { SDLK_PRINTSCREEN, 10, 5 },
    { SDLK_SCROLLLOCK, 10, 6 },
    { SDLK_PAUSE, 10, 7 },

    // Byte 11: numpad left column + top
    { SDLK_NUMLOCKCLEAR, 11, 0 },
    { SDLK_KP_DIVIDE, 11, 1 },
    { SDLK_KP_MULTIPLY, 11, 2 },
    { SDLK_KP_MINUS, 11, 3 },
    { SDLK_KP_7, 11, 4 }, { SDLK_KP_8, 11, 5 }, { SDLK_KP_9, 11, 6 },
    { SDLK_KP_PLUS, 11, 7 },

    // Byte 12: numpad middle/bottom
    { SDLK_KP_4, 12, 0 }, { SDLK_KP_5, 12, 1 }, { SDLK_KP_6, 12, 2 },
    { SDLK_KP_1, 12, 3 }, { SDLK_KP_2, 12, 4 }, { SDLK_KP_3, 12, 5 },
    { SDLK_KP_0, 12, 6 }, { SDLK_KP_PERIOD, 12, 7 },

    // Byte 13: numpad Enter + reserved
    { SDLK_KP_ENTER, 13, 0 },
    // (IDs 105-127 free for media keys, international keys, etc.)

    // Terminator
    { 0, 0, 0 } 
};

// Here comes the fun part. This is where we start emulating the CPU.
// Step moves through a single CPU cycle, interprets opcodes for
// instructions, and resolves them to real behaviors.
static void step(void) {
    if (cpu.halted) {return;}                                           // If cpu is halted, do nothing and go back
    if (cpu.fatal) {reset_cpu(&cpu); return;}                           // On fatal error, reset cpu
    if (cpu.error) {cpu.error = 0;                                      // On nonfatal error, clear error,
        printf("Nonfatal error: %s\n", cpu.errmsg);}                    // then print error message to terminal.
    uint32_t addr = get_pc(&cpu);                                       // Install our cpu lmao (or, well, read pc into a local really)
    uint32_t word1 = build_16(                                          // Initially, load only one word of instruction.
        mem_read(addr & 0xFFFFFF),                                      // Mask each read within legal range
        mem_read((addr + 1) & 0xFFFFFF));                               // Increment and read second byte
    uint32_t word2 = 0;                                                 // Second word is not even loaded.
    uint8_t opcode  = (word1 >> 12) & 0xF;                              // Fetches the high nibble of word 1 byte 1
    uint8_t subcode = (word1 >>  8) & 0xF;                              // Fetches the low  nibble of word 1 byte 1

    // This section is solely for convenience in the emulator;
    // on real hardware, each of these nibbles would be directly
    // processed as basically just bitmaps and their use case 
    // would be determined by the instruction they feed into.
    // For my use case, being able to directly query each nibble
    // makes implementing things easier.
    uint8_t n2 = subcode;
    uint8_t n3      = (word1 >>  4) & 0xF;
    uint8_t n4      =  word1        & 0xF;
    if ((opcode > 0x0000) && (opcode < 0x0006)) {                       // If opcode is 1, 2, 3, 4, or 5, build second word
        word2 = build_16(mem_read((addr + 2) & 0xFFFFFF),               //  get first  byte of second word
        mem_read((addr + 3) & 0xFFFFFF));                               //  get second byte of second word
    } set_pc(&cpu, addr + ((opcode >= 0x1 && opcode <= 0x5) ? 4 : 2));  // Increment the program counter
    switch (opcode) {                                                   // alright, time to implement some instructions!
        case 0x0:                                                       // subcode prefix, so process subcodes first
            switch (subcode) {                                          // subcode handle switch
                case 0x0: break;                                        // NOP
                case 0x1: cpu.halted = 1; break;                        // HALT
                case 0x2: cpu.r[n3] = mem_read (cpu.r[n4]);     break;  // LOAD
                case 0x3: mem_write(cpu.r[n3],  cpu.r[n4]);     break;  // STORE
                case 0x4: cpu.r[n3] += cpu.r[n4];               break;  // ADD
                case 0x5: cpu.r[n3] *= cpu.r[n4];               break;  // MUL
                case 0x6: cpu.r[n3] -= cpu.r[n4];               break;  // SUB
                case 0x7: if (cpu.r[n4] == 0) {                         // DIV
                    cpu.r[n3] = 0xFFFFFFFF;                     break;  // DIV/0 = 0xFFFFFFFF
                }         cpu.r[n3] /= cpu.r[n4];               break;  // Normal DIV behavior
                case 0x8: if (cpu.r[n4] == 0) {                         // MOD
                    cpu.r[n3] = 0x00000000;                     break;  // MOD/0 = 0x00000000
                }         cpu.r[n3] %= cpu.r[n4];               break;  // Normal MOD behavior
                case 0x9: cpu.r[n3] &= cpu.r[n4];               break;  // AND
                case 0xA: cpu.r[n3] |= cpu.r[n4];               break;  // OR
                case 0xB: cpu.r[n3] ^= cpu.r[n4];               break;  // XOR
                case 0xC: cpu.r[n3] = ~cpu.r[n3];               break;  // NOT
                case 0xD: cpu.r[n3]  = cpu.r[n4];               break;  // MOV
                case 0xE: cpu.r[n3] = get_pc(&cpu);             break;  // MOVPC
                case 0xF: if (n4 == 0x0) {                              // JMPR
                    set_pc(&cpu, (cpu.r[n3] & 0xFFFFFF));       break;  //  jumps to address specified in register
                } else if (n4 == 0xF) {                                 // CALLR
                    uint16_t pclo = ((cpu.pc << 16) >> 16);             //  low 16 of PC
                    uint16_t pchi =  (cpu.pc >> 16);                    //  high 8 of PC
                    push_to_stack(pclo);                                //  Push low
                    push_to_stack(pchi);                                //  Push high
                    set_pc(&cpu, cpu.r[n3]);                    break;  //  Jump to target address in register
                } else {                                                // you done fucked up
                    cpu.fatal = 1; cpu.errmsg = "bad JMPR/CALLR flag";  // throw an error and reset cpu
                } break;                                                // then gtfo of there
            } break;                                                    //
        case 0x1: cpu.r[n2] = build_24(build_8(n3, n4), word2); break;  // LOADI
        case 0x2: set_pc(&cpu,build_24(build_8(n3,n4), word2)); break;  // JMP
        case 0x3: if (cpu.r[n2] == 0) {                                 // JZ
            set_pc(&cpu, build_24(build_8(n3, n4), word2)); }   break;  //
        case 0x4: if (cpu.r[n2] != 0) {                                 // JNZ
            set_pc(&cpu, build_24(build_8(n3, n4), word2)); }   break;  //
        case 0x5: {                                                     // CALL
            uint16_t pclo = ((cpu.pc << 16) >> 16);                     //  low 16 of PC
            uint16_t pchi =  (cpu.pc >> 16);                            //  high 8 of PC
            push_to_stack(pclo);                                        //  Push low
            push_to_stack(pchi);                                        //  Push high
            set_pc(&cpu, build_24(build_8(n3, n4), word2));     break;  //  Jump to imm24
        } case 0x6: {                                                   // RET
            uint8_t  pchi = pop_from_stack();                           //  pop high 8 of PC
            uint16_t pclo = pop_from_stack();                           //  pop low 16 of PC
            set_pc(&cpu, build_24(pchi, pclo));                 break;  //  reconstruct 24-bit PC
        } case 0x7: {                                                   // SHL
            uint32_t shamt;                                             //  shamt == shift amount
            if (n3 & 0x8) shamt = cpu.r[n4];                            //  indirect; shamt become register value
            else          shamt = ((n3 << 2) | (n4 >> 2)) & 0x3F;       //  direct;   shamt become 6-bit immediate
            if (shamt > 31) cpu.r[n2] = 0;                              //  shamt over 32 zeroes register
            else {cpu.r[n2] <<= shamt;}                         break;  //  Any other shamt is applied to register
        } case 0x8: {                                                   // SHR
            uint32_t shamt;                                             //  shamt == shift amount
            if (n3 & 0x8) shamt = cpu.r[n4];                            //  indirect; shamt become register value
            else          shamt = ((n3 << 2) | (n4 >> 2)) & 0x3F;       //  direct;   shamt become 6-bit immediate
            if (shamt > 31) cpu.r[n2] = 0;                              //  shamt over 32 zeroes register
            else {cpu.r[n2] >>= shamt;}                         break;  //  Any other shamt is applied to register
        } case 0x9: if (cpu.r[n2] < cpu.r[n3]) {                        // JLT
            set_pc(&cpu, (cpu.r[n4] & 0xFFFFFF));   }           break;  //  mask to 24-bits & use for jump
        case 0xA: if (cpu.r[n2] > cpu.r[n3])   {                        // JGT
            set_pc(&cpu, (cpu.r[n4] & 0xFFFFFF));   }           break;  //  mask to 24-bits & use for jump
        case 0xB: {                                                     // RND
            uint32_t imm8 = (n3 << 4) | n4;                             //  load immediate as variable
            uint32_t result = random_cell;                              //  read current state before advancing
            random_cell ^= random_cell << 13;                           //  Advance the hardware random number generator. 
            random_cell ^= random_cell >> 17;                           //  The 2410 uses an xorshift method to generate
            random_cell ^= random_cell << 5;                            //  32-bit pseudorandom numbers. 
            if (random_cell == 0) random_cell = 0xDEADBEEF;             //  If register is ever zeroed, we must reseed, or we shall never generate another number again. 
            if (imm8 == 0 || imm8 == 32) {                              //  if immediate is 0 or 32:
                cpu.r[n2] = result;                                     //     full 32 bits
            } else if (imm8 < 32) {                                     //  if immediate is between 1 and 31:
                uint32_t mask = (1u << imm8) - 1;                       //     low imm8 bits
                cpu.r[n2] = result & mask;                              //  Mask the result
            } else {                                                    //  If anything else happened, shit's fucked
                cpu.fatal = 1;                                          //  Throw a CPU error, force a reset
                cpu.errmsg = "RND width > 32"; }                break;  //  Random width too girth
        } case 0xC: {                                                   // PUSH
            push_to_stack(cpu.r[n2] & 0xFFFF);                          //  low 16
            push_to_stack((cpu.r[n2] >> 16) & 0xFFFF);          break;  //  high 16
        } case 0xD: {                                                   // POP
            uint32_t hi = pop_from_stack();                             //  high 16
            uint32_t lo = pop_from_stack();                             //  low 16
            cpu.r[n2] = (hi << 16) | lo;                        break;  //  reconstruct full value
        } case 0xE: write_char(                                         // WCHR
            cpu.r[n2],                                                  //  write character at address specified n1's register,
            build_16((cpu.r[n3] & 0xFF), (cpu.r[n4] & 0xFF)) ); break;  //  using palettes specified in n3, n4's registers.
        case 0xF: {                                                     // RCHR
            uint16_t cashcell = read_char(cpu.r[n2]);                   //  Get 16 bit word from character buffer
            cpu.r[n3] = (cashcell >> 8);                                //  Get attribute byte
            cpu.r[n4] = (cashcell & 0xFF);                      break;  //  Get character byte
        } 
    } 
}

// GPU setup and emulation
// lord save me, this is a new one
// ain't never tried this before, but
// well, fuck it, I guess, why not try?
typedef struct {
    int mode;                 // 0 = PAL, 1 = NTSC
    int pixels_per_line;      // 640 or 636
    int lines_per_frame;      // 312 or 262
    int pixel_x;              // 0 .. pixels_per_line - 1
    int line_y;               // 0 .. lines_per_frame - 1
    bool hblank;              // pixel_x >= 480
    bool vblank;              // line_y >= 240
    bool frame_toggle;        // toggles at start of vblank
    bool cursor_on;           // cursor visible on this frame
    uint32_t frame_count;     // for cursor blink; increments per frame
    uint32_t linebuf[640];    // one scanline of RGB, active area only (480 used)
} GPU;

static GPU gpu;                                                             // Install our GPU, it's kind of a little important
static uint32_t framebuf[480 * 240];                                        // ARGB Framebuffer, active line only
static SDL_Renderer *g_ren = NULL;                                          // I accidentally wrote "Tenderer" here at first
static SDL_Texture  *g_tex = NULL;                                          // anyways pull a pointer to texture and renderer from main()
static void blit_line_to_frame(int y, const uint32_t *line) {               // Send a line to the framebuffer
    memcpy(&framebuf[y * 480], line, 480 * sizeof(uint32_t));               // Copy 480 32-bit integers from line into framebuffer, starting at y
}                                                                           // Alright, that's the main bulk of pre-setup. Next...

// Alright boys, wish me luck. I've gotta start writing SDL code from here on out, and, dammit, I don't fully get it, but I gotta, so... Here goes.
static void present_frame(void) {                                           // Present a frame to SDL
    SDL_UpdateTexture(g_tex, NULL, framebuf, 480 * sizeof(uint32_t));       // Take target texture, draw a rect to fill it, and pass pixels from frame buffer to the draw
    SDL_RenderClear(g_ren);                                                 // Fill render target with draw color to clear screen
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);                               // Draw texture to renderer target, whole texture, scaling to fill whole target
    SDL_RenderPresent(g_ren);                                               // Take all that shit we just did and tell the renderer to actually present it to the screen
}                                                                           // Sorry if these comments are inaccurate, SDL is still esoteric to me, especially the newer SDL2. 
// I used to use SDL1.2 a lot, but I kinda just copied things online without really understanding them.. This is in large part a learning project, so I want to bridge that gap if I can.
// Anyways, apologies for the digression. Back to, y'know, making an entire godddamn GPU emulator. Fuck me.

static void init_gpu(GPU *g, int mode) {                                    // Initialize the GPU state
    g->mode = mode;                                                         // Set output mode PAL or NTSC
    g->pixels_per_line = (mode == 0) ? 640 : 636;                           // If pal, 640, if ntsc, 636
    g->lines_per_frame = (mode == 0) ? 312 : 262;                           // If pal, 312, if ntsc, 262
    g->pixel_x      =     0;                                                // pixel_x      always starts = 0
    g->line_y       =     0;                                                // line_y       always starts = 0
    g->hblank       = false;                                                // hblank       always starts = 0
    g->vblank       = false;                                                // vblank       always starts = 0
    g->frame_toggle = false;                                                // frame_toggle always starts = 0
    g->cursor_on    =  true;                                                // cursor_on    always starts = 1
    g->frame_count  =     0;                                                // frame_count  always starts = 0
}                                                                           // hehe. i like pretty comments ^v^
static uint32_t gpu_pixel(int sx, int sy) {
    gpu.hblank = gpu.pixel_x >= 480;                                        // Initialize hblank
    gpu.vblank = gpu.line_y >= 240;                                         // Initialize blank
    
    int cell_x   = sx >> 3;                                                 // Shift right 3 sx to get cell pos
    int cell_y   = sy >> 3;                                                 // Shift right 3 sy to get line pos
    int cell     = cell_y * 60 + cell_x;                                    // Get currently active cell from bitshifted sx and sy
    int px       = sx & 0x7;                                                // Mask X to a pixel within the current cell
    int py       = sy & 0x7;                                                // Mask Y to a pixel within the current cell

    uint16_t c1  = charbuffer1[cell];                                       // Character from character buffer 1
    uint16_t c2  = charbuffer2[cell];                                       // Character from character buffer 2
    uint8_t  g1  = c1 & 0xFF;                                               // Glyph byte from character 1
    uint8_t  g2  = c2 & 0xFF;                                               // Glyph byte from character 2
    uint8_t  p1  = (c1 >> 8) & 0x3;                                         // Palette bits from character 1
    uint8_t  p2  = (c2 >> 8) & 0x3;                                         // Palette bits from character 2

    uint8_t fb1  = fontbuffer[g1 * 32 + py * 4 + (px >> 1)];                // Four bits per pixel, high nibble left
    uint8_t fb2  = fontbuffer[g2 * 32 + py * 4 + (px >> 1)];                // Perform same op for second char                                       __________________________________________________________
    uint8_t pix1 = (px & 0x1) ? (fb1 & 0xF) : (fb1 >> 4);                   // If px odd then right nibble, just mask to low 4 bits.                / These comments are documenting a shared behavior between/
    uint8_t pix2 = (px & 0x1) ? (fb2 & 0xF) : (fb2 >> 4);                   // If px even, though, shift right by 4 to get high 4 into low nibble. /  both of these lines, not a difference in bevior.       /
    
    bool cursor_here = (gpucontrol[3] & 0x1) &&                             // If cursor is enabled,
                       (cell_x == gpucontrol[1]) &&                         // and this cell's horizontal position matches cursor's x position,
                       (cell_y == gpucontrol[2]) &&                         // and this cell's vertical   position matches cursor's y position,
                       gpu.cursor_on;                                       // then turn on the cursor at this cell.
    bool cursor_plane2 = (gpucontrol[3] >> 2) & 0x1;                        // Extract cursor plane select
    if (cursor_here && !cursor_plane2) pix1 ^= 0xF;                         // If cursor here and plane select is 0 render on plane 1
    if (cursor_here &&  cursor_plane2) pix2 ^= 0xF;                         // If cursor here and plane select is 1 render on plane 2

    uint32_t out = gpucontrol[6];                                           // BORDER default
    bool plane1  = gpucontrol[0] & 0x1;                                     // Is plane 1 enabled?
    bool plane2  = gpucontrol[0] & 0x2;                                     // Is plane 2 enabled?

    if (plane1) out = palbuffer1[p1 * 16 + pix1];                           // If plane 1 is enabled, set out var as pixel 1
    if (plane2) {                                                           // If plane 2 is enabled, we have more work to do
        bool transparent;                                                   // Pixel 2 is transparent if high
        if (cursor_here && cursor_plane2) transparent = false;              // If cursor is here, bypass transparency
        else {                                                              // Otherwise we really do have to manage transparency
            uint8_t bitmap = palbuffer2[64 + (p2 * 16 + pix2) / 8];         // Offset 64 into palette buffer to reach transparency bitmap, then (p2 * 16) to find the specific palette's bitmap
            transparent = (bitmap >> ((p2 * 16 + pix2) & 0x7)) & 1;         // Pixel here is transparent it bitmap overlaps with the color id of this cell specifically.. I think. Honestly I don't fully understand this one.
        } if (!transparent) out = palbuffer2[p2 * 16 + pix2];               // But anyways, basically, if not transparent, overwrite output with pix2 data.
    }
    uint32_t r8 = ((out >> 5) & 0x7) * 255 / 7;                             // Get RRR
    uint32_t g8 = ((out >> 2) & 0x7) * 255 / 7;                             // Get GGG
    uint32_t b8 =  (out & 0x3) * 255 / 3;                                   // Get BB
    out = 0xFF000000 | (r8 << 16) | (g8 << 8) | b8;                         // Construct AARRGGBB color for SDL
    return out;                                                             // Return output color for this pixel
}
static void gpu_tick(void) {                                                // Just like the step function, this simulates one GPU cycle
    if (gpu.pixel_x < 480 && gpu.line_y < 240) {                            // If scanout position within screenspace
        int sx = (gpu.pixel_x + (gpucontrol[4] & 0x7)) % 480;               // Scroll X
        int sy = (gpu.line_y  + (gpucontrol[5] & 0x7)) % 240;               // Scroll y
        gpu.linebuf[gpu.pixel_x] = gpu_pixel(sx, sy);                       // Load pixel val into linebuffer
    } gpu.pixel_x += 1;                                                     // Increment scanout position by 1
    if (gpu.pixel_x >= gpu.pixels_per_line) {                               // If outside pixels per line, 
        gpu.pixel_x = 0;                                                    // print 0 / border color
        if (gpu.line_y < 240) {                                             // If within lines per frame,
            blit_line_to_frame(gpu.line_y, gpu.linebuf);                    // Send line to screen, 
        } gpu.line_y += 1;                                                  // Then increment scanline.
        if (gpu.line_y >= gpu.lines_per_frame) {                            // If outside lines per frame,
            gpu.line_y = 0;                                                 // print 0 /border color,
            gpu.frame_toggle = !gpu.frame_toggle;                           // Invert value of frame_toggle,
            gpu.frame_count += 1;                                           // Increment framecount,
            present_frame();                                                // Then hand the completed frame to SDL.
        } status[0] = (gpu.vblank      ? 0x01 : 0) |                        // Update vblank status
                      (gpu.hblank      ? 0x02 : 0) |                        // Update hblank status
                      (gpu.frame_toggle? 0x04 : 0) |                        // Update frame toggle
                      (gpu.mode        ? 0x08 : 0);                         // Update the frame toggle
                                                                            // ...Oh my god, I think I just implemented a GPU? I mean, I'm totally skipping over the vblank and hblank stuff...
    }                                                                       // ...But I actually think that this logic would probably translate mostly to a real VGA card, if only an excessively terrible one...
}                                                                           // ...Maybe a thing to consider for the future if I ever wanted to use an FPGA to simulate this chip instead of software emulation?

static void load_file(const char *path, void *buf, size_t sz) {             // Load a binary file into a memory bank
    FILE *f = fopen(path, "rb");                                            // f is a pointer to a file opened by fopen from filepath
    if (!f) return;                                                         // f failed to load; initialize memory bank as all zeroed
    fread(buf, 1, sz, f);                                                   // Otherwise, read into memory buffer all that fits in sz
    fclose(f);                                                              // Close the file
} static void save_file(const char *path, const void *buf, size_t sz) {     // Save a memory bank's state to a binary file
    FILE *f = fopen(path, "wb");                                            // f is a pointer to a file opened by fopen from filepath
    if (!f) return;                                                         // f failed to load...  return w/o recording memory state
    fwrite(buf, 1, sz, f);                                                  // Otherwise, write binary file = all memory buffer in sz
    fclose(f);                                                              // Close the door, you weren't born in a barn.
} static void load_persistent(void) {                                       // Initialize persistent memory banks
    load_file(ROM_FILE,     rom,     sizeof(rom));                          // ROM
    load_file(PMEM1_FILE,   pmem1,   sizeof(pmem1));                        // PMEM1
    load_file(PMEM2_FILE,   pmem2,   sizeof(pmem2));                        // PMEM2
    load_file(EXTPMEM_FILE, extpmem, sizeof(extpmem));                      // EXTPMEM
} static void save_persistent(void) {                                       // Save persistent memory banks to files
    save_file(PMEM1_FILE,   pmem1,   sizeof(pmem1));                        // PMEM1
    save_file(PMEM2_FILE,   pmem2,   sizeof(pmem2));                        // PMEM2
    save_file(EXTPMEM_FILE, extpmem, sizeof(extpmem));                      // EXTPMEM
}

int main(int argc, char **argv) {                                           // Start the main loop with these two pointers... Then, cast them to the void.
    (void)argc; (void)argv;                                                 // Yes, GCC, I am aware that they do nothing and are unused. Fuck off.

    if (SDL_Init(SDL_INIT_VIDEO) != 0 )                     {               // Initialize SDL video
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());                  // Print standard error and SDL error
        return 1;                                           }               // End program
    SDL_Window *win = SDL_CreateWindow("Helio 210",                         // Create the window
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,                     // Center the window
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);                                    // Size is WIN_W * WIN_H
    if (!win) {                                                             // If no window pointer
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());          // Then we have no window
        return 1;                                                           // So, like, throw an error
    }                                                                       // or somethin like that, y'know?
    g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);          // Initialize hardware accelerated renderer if possible
    if (!g_ren) g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE); // If not, then try to make a software renderer
    if (!g_ren)                                                   {         // If *that* fails, um. Are you running this on a toaster?   _____________________
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());        // Throw an error, apparently you have no software          / How are you even   /
        return 1;                                                 }         // Or even monitor                                         / reading this code? /
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");                        // Nearest-neighbor scaling. Sweet jesus that's a long... I hate that. But... Whatever.  
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,              // Initialize the texture global..
                              SDL_TEXTUREACCESS_STREAMING, 480, 240);       // God, I hate how long the words or whatever they're called are in SDL, but it's a very useful graphics library, so, I'll try to just ignore it.
    if (!g_tex) {                                                           // Y'ain't got no texture, smh.
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());         // Throw an error and give up.
        return 1;                                                           // We can't do nothin for ya.
    }                                                                       // Sorry, bud.
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);                            // Default/border color.

    load_persistent();                                                      // ROM, PMEM1, PMEM2, EXTPMEM
    load_file(DEFAULT_FONT, fontbuffer, sizeof(fontbuffer));                // Initialize the font buffer too
    load_file("memory/palette.bin", palbuffer1, sizeof(palbuffer1));        // and the default palette buffer.
    memcpy(palbuffer2, palbuffer1, sizeof(palbuffer1));                     // Plane 2 copies Plane 1's palette buffer by default, with zero transparency bits set.
    if (rom[0] == 0 && rom[1] == 0 && rom[2] == 0 && rom[3] == 0) {         // Just a little diagnostic check; an all-zero ROM generally means
        fprintf(stderr,"Warning: ROM appears empty (no program loaded).\n");// "You have not assembled anything yet", so refuse to boot. 
        fprintf(stderr, "  Expected file: %s\n", ROM_FILE);                 // After all, you'd otherwise just cause a near-instant illegal memory access 
    }                                                                       // and enter an infinite reset loop, so, why not spare us the crashage?

    init_cpu(&cpu);                                                         // Install our CPU
    init_gpu(&gpu, 0);                                                      // Install our GPU, set PAL to 0
    acia_config_t acia_cfg = {                                              // Tell the ACIA who we really are
        .cpu_hz      = 10000000,                                            // 10 MHz CPU
        .periph_div  = 8,                                                   // 1.25 MHz peripheral bus
        .num_units   = 2,                                                   // UART0 + UART1 
        .port_base   = 9000};                                               // 127.0.0.1:9000 and :9001
    acia_init(&acia_cfg);                                                   // Install our ACIAs
    psg_config_t psg_cfg = {                                                // Introduce ourselves to the audio chip
        .phi_hz      = 1250000,                                             // CPU / 8
        .chip_div    = 8,                                                   // psg_tick called every 8 CPU cycles
        .sample_rate = 48000,                                               // sample rate
        .tone_div    = 0,                                                   // default 16
        .env_div     = 0};                                                  // default 256
    psg_sdl_open(&psg_cfg);                                                 // Install our AY-3-8910 programmable sound generator
    random_cell      = SDL_GetTicks() | 1;                                  // Seed some entropy for the RNG
    last_clock_tick = SDL_GetTicks();                                       // Initialize system clock
    
    int running = 1;                                                        // Main loop in a main loop, whodathunkit
    const double TARGET_HZ    = 10000000.0;                                 // Target clock speed
    uint64_t last_time        = SDL_GetPerformanceCounter();                // Last time
    uint64_t perf_freq        = SDL_GetPerformanceFrequency();              // Performance frequency
    double cycle_accumulator  = 0.0;                                        // Cycle accumulator
    uint32_t last_save        = SDL_GetTicks();                             // Last save time
    while (running) {                                                       // Aight, the loop now.
        SDL_Event e;                                                        // e is SDL's internal event queue, I think.
        while (SDL_PollEvent(&e)) {                                         // This drains it out, one little thing at a time.
            if (e.type == SDL_QUIT) { running = 0; break; }                 // If it gets a QUIT event, break the main loop.
            if (e.type == SDL_KEYDOWN) {                                    // If it is a keydown event, we have some work to do.
                if (e.key.keysym.sym == SDLK_ESCAPE) { running = 0; break;} // Check first if the key pressed is the escape key and, if so, break the main loop.
                for (int i = 0; key_mappings[i].sym != 0; i++)      {       // Look up the key's place in the keyboard map
                    if (key_mappings[i].sym == e.key.keysym.sym)    {       // If we found something valid, then...
                        keymap[key_mappings[i].byte_idx] |=                 //  Set the correct bit for that key
                            (1u << key_mappings[i].bit_idx);                //  to high in the bitmap
                        break;                                      }}      //  then break out of there.
            } if (e.type == SDL_KEYUP) {                                    // Now then. If it's a keyup, we basically just have to do the same shit.
                for (int i = 0; key_mappings[i].sym != 0; i++)      {       // Look up the key's place in the keyboard map
                    if (key_mappings[i].sym == e.key.keysym.sym)    {       // If we found something valid, then...
                        keymap[key_mappings[i].byte_idx] &=                 //  Set the correct bit for that key
                            ~(1u << key_mappings[i].bit_idx);               //  to low in the bitmap
                        break;                                      }}      //  then break out of there.
            }} if (!running) break;                                         // Anyways, you got this far? We left, man. We're done. Wrap it up. Move on to other shit, like we're doing.

        uint64_t now     = SDL_GetPerformanceCounter();                     // Write the current down somewhere, uhhh, call it now. No don't call it now, just call it "now". Smh.
        double   elapsed = (double)(now - last_time) / (double)perf_freq;   // Elapsed time between now and the last time we checked.. Divide the result by the performance frequency.
        last_time = now;                                                    // Last time is now now. Don't think about it too much
        cycle_accumulator += elapsed * TARGET_HZ;                           // Accumulate a cycle girls. No I don't mean it that way-
        if (cycle_accumulator > 500000.0) cycle_accumulator = 500000.0;     // Prevent possible overflow error..
        int cycles = (int)cycle_accumulator;                                // Cast cycle_accumulator's value to an integer and store the result as cycles
        cycle_accumulator -= cycles;                                        // Then subtract cycles from the accumulator.
        if (cycles > 500000) cycles = 500000;                               // Oh, but, um. Don't run too many. Like, sure, 49.9999 million cycles in a second, like, I can accept that, but 50k? Seriously? Lay off.
        for (int i = 0; i < cycles; i+= 1) {                                // Run one cycle for every module.
            step();                                                         // CPU
            gpu_tick();                                                     // GPU
            acia_tick();                                                    // ACIA
            psg_tick();}                                                    // PSG
        acia_poll();                                                        // Let the ACIA sockets do some work, just once per iteration. As a treat.
        uint32_t now_ms      = SDL_GetTicks();                              // Get the current millisecond from GetTicks()
        uint32_t elapsed_ms  = now_ms - last_clock_tick;                    // Find elapsed milliseconds by subtracting current from last clock tick.
        if (elapsed_ms >= 10)              {                                // If greater than or equal to 10...
            system_clock += elapsed_ms / 10;                                //  Increment centisecond counter
            last_clock_tick += (elapsed_ms / 10) * 10; }                    //  increment last clock tick by ten times elapsed centiseconds
        if (now_ms - last_save >= SAVE_INTERNAL_MS)    {                    // If SAVE_INTERNAL_MS has passed, then    
            save_persistent(); last_save = now_ms;     }                    // Save our PMEM banks to BIN files, keep em backed up
        if (cycles == 0) SDL_Delay(1);                                      // If we're fasting runner than real-time, sleep 1ms to avoid burning a CPU core doing nothing
    }                                                                       // By gosh and by golly, that's a computer emulator honey
    
    // Let's just tidy up around here, then we're done. Not as horribly complicated as I expected it to be.. I'm pretty pleased with it. 
    // Hope my comments are actually useful to anyone reading this, but, well, if anyone besides me is reading this, I will be very, very surprised...'
    // Sorry, that was a bit of a tangent. Anyways. Let's just clean up around here a little bit, shall we?
    save_persistent();                                                      // Save our PMEM banks to BIN files
    psg_sdl_close();                                                        // Shut down the SDL_Audio instance in the PSG
    acia_shutdown();                                                        // Shut down the ADIAs
    SDL_DestroyTexture(g_tex);                                              // Destroy our textures
    SDL_DestroyRenderer(g_ren);                                             // And our renderer
    SDL_DestroyWindow(win);                                                 // Even the window.
    SDL_Quit();                                                             // Well. It's been fun, lads and lasses.
    return 0;                                                               // Have a good day, and have fun with my ridiculous computer perhaps.
}                                                                           // Who knows. Maybe it's useful for something.