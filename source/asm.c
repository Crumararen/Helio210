// asm.c — Helio 213 Assembler
// Build: gcc -O2 -o asm asm.c
// Usage: ./asm <input.asm> <output.bin> [size_flag]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_LABELS 1024
#define MAX_FIXUPS 2048
#define MAX_LINE_LENGTH 256

// --- SYSTEM MEMORY MAP ---
typedef struct {
    const char *name;
    uint32_t start;
    uint32_t end;
    int is_protected;
} Region;

Region REGIONS[] = {
    {"ROM",     0x00000, 0x01FFF, 0},
    {"STACK",   0x02000, 0x027FF, 1},
    {"RAM",     0x02840, 0x1283F, 0},
    {"CHRBUF1", 0x12840, 0x12F47, 1},
    {"PALBUF1", 0x12F48, 0x12F87, 0},
    {"CHRBUF2", 0x12F88, 0x1368F, 1},
    {"PALBUF2", 0x13690, 0x136D7, 0},
    {"GPUCTRL", 0x136D8, 0x136DF, 0},
    {"FONT",    0x136E0, 0x156DF, 0},
    {"PMEM1",   0x156E0, 0x1D6DF, 0},
    {"PMEM2",   0x1D6E0, 0x256DF, 0},
    {"EXTPMEM", 0x256E0, 0x356DF, 0}
};
const int NUM_REGIONS = sizeof(REGIONS) / sizeof(REGIONS[0]);

// --- INSTRUCTION LOOKUP TABLE ---
typedef struct {
    const char *mnemonic;
    uint8_t opcode;
    int num_ops;
    const char *format;
    uint8_t flag;
} Instruction;

Instruction INSTRUCTIONS[] = {
    {"LOADI", 0x10, 2, "imm24_reg", 0},
    {"JZ",    0x30, 2, "imm24_reg", 0},
    {"JNZ",   0x40, 2, "imm24_reg", 0},
    {"JMP",   0x20, 1, "imm24", 0},
    {"CALL",  0x50, 1, "imm24", 0},
    {"RET",   0x60, 0, "reg0", 0},
    {"SHL",   0x70, 2, "shift", 0},
    {"SHR",   0x80, 2, "shift", 0},
    {"JLT",   0x90, 3, "reg3", 0},
    {"JGT",   0xA0, 3, "reg3", 0},
    {"RND",   0xB0, 2, "imm3", 0},
    {"PUSH",  0xC0, 1, "reg1", 0},
    {"POP",   0xD0, 1, "reg1", 0},
    {"WCHR",  0xE0, 3, "reg3", 0},
    {"RCHR",  0xF0, 3, "reg3", 0},
    {"NOP",   0x00, 0, "sub_0reg", 0},
    {"HALT",  0x01, 0, "sub_0reg", 0},
    {"LOAD",  0x02, 2, "sub_2reg", 0},
    {"STORE", 0x03, 2, "sub_2reg", 0},
    {"ADD",   0x04, 2, "sub_2reg", 0},
    {"MUL",   0x05, 2, "sub_2reg", 0},
    {"SUB",   0x06, 2, "sub_2reg", 0},
    {"DIV",   0x07, 2, "sub_2reg", 0},
    {"MOD",   0x08, 2, "sub_2reg", 0},
    {"AND",   0x09, 2, "sub_2reg", 0},
    {"OR",    0x0A, 2, "sub_2reg", 0},
    {"XOR",   0x0B, 2, "sub_2reg", 0},
    {"NOT",   0x0C, 1, "sub_1reg", 0},
    {"MOV",   0x0D, 2, "sub_2reg", 0},
    {"MOVPC", 0x0E, 1, "sub_1reg", 0},
    {"JMPR",  0x0F, 1, "sub_jmpr", 0x0},
    {"CALLR", 0x0F, 1, "sub_jmpr", 0xF},
};
const int NUM_INSTRUCTIONS = sizeof(INSTRUCTIONS) / sizeof(INSTRUCTIONS[0]);

// --- ASSEMBLER STATE ---
typedef struct {
    char name[64];
    uint32_t addr;
} Label;

typedef struct {
    uint32_t offset;
    char label[64];
    int num_bytes;
    uint32_t mask;
    int line_num; // [NEW] Track the line where this fixup was created
} Fixup;

uint8_t *output_buffer = NULL;
uint32_t output_buffer_size = 0;
uint32_t current_org = 0;
Region *current_region = &REGIONS[0];
uint32_t output_offset = 0;
int error_flag = 0;
int current_line_number = 1; // [NEW] Global line counter

Label labels[MAX_LABELS];
int label_count = 0;

Fixup fixups[MAX_FIXUPS];
int fixup_count = 0;

// --- ERROR HANDLING ---
void print_error(const char *msg) {
    // [CHANGED] Now prints the line number!
    fprintf(stderr, "ERROR (Line %d): %s\n", current_line_number, msg);
    error_flag = 1;
}

// --- HELPERS ---
uint32_t find_label(const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) return labels[i].addr;
    }
    return 0xFFFFFFFF; // NOT_FOUND
}

void define_label(const char *name, uint32_t addr) {
    if (label_count >= MAX_LABELS) {
        print_error("Too many labels");
        return;
    }
    strncpy(labels[label_count].name, name, 63);
    labels[label_count].addr = addr;
    label_count++;
}

int is_register(const char *s) {
    if (strlen(s) < 2) return 0;
    if (s[0] != 'R' && s[0] != 'r') return 0;
    for (int i = 1; s[i]; i++) if (!isdigit(s[i])) return 0;
    return 1;
}

int parse_register(const char *s) {
    if (!is_register(s)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Invalid register: %s", s);
        print_error(msg);
        return 0;
    }
    int num = atoi(s + 1);
    if (num < 0 || num > 15) {
        print_error("Register out of range (R0-R15)");
        return 0;
    }
    return num;
}

uint32_t parse_number(const char *s) {
    if (s[0] == '\'') return (uint32_t)s[1]; // Char literal
    if (strlen(s) > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return strtoul(s, NULL, 16);
    if (strlen(s) > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) return strtoul(s + 2, NULL, 2);
    return strtoul(s, NULL, 10);
}

int is_number(const char *s) {
    if (s[0] == '\'') return 1;
    if (strlen(s) > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X' || s[1] == 'b' || s[1] == 'B')) return 1;
    for (int i = 0; s[i]; i++) if (!isdigit(s[i])) return 0;
    return 1;
}

void emit_byte(uint8_t val) {
    if (output_offset >= output_buffer_size) {
        print_error("Output buffer overflow (exceeded max size clamp)");
        return;
    }
    output_buffer[output_offset++] = val;
}

uint32_t resolve_or_fixup(const char *arg, uint32_t max_val, int fixup_bytes, uint32_t fixup_mask) {
    uint32_t label_addr = find_label(arg);
    if (label_addr != 0xFFFFFFFF) {
        if (label_addr > max_val) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Symbol %s exceeds field limit", arg);
            print_error(msg);
        }
        return label_addr & max_val;
    }

    if (is_number(arg)) {
        uint32_t val = parse_number(arg);
        if (val > max_val) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Value %s exceeds limit", arg);
            print_error(msg);
        }
        return val & max_val;
    }

    if (fixup_bytes == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Undefined symbol: %s", arg);
        print_error(msg);
        return 0;
    }

    if (fixup_count >= MAX_FIXUPS) {
        print_error("Too many fixups");
        return 0;
    }

    fixups[fixup_count].offset = output_offset;
    strncpy(fixups[fixup_count].label, arg, 63);
    fixups[fixup_count].num_bytes = fixup_bytes;
    fixups[fixup_count].mask = fixup_mask;
    fixups[fixup_count].line_num = current_line_number; // [NEW] Save the line number
    fixup_count++;
    return 0;
}

Instruction* lookup_instruction(const char *mnemonic) {
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        if (strcasecmp(INSTRUCTIONS[i].mnemonic, mnemonic) == 0) return &INSTRUCTIONS[i];
    }
    return NULL;
}

// --- MAIN ASSEMBLY LOGIC ---
void assemble_line(char *line) {
    // Strip comments
    char *comment = strchr(line, ';');
    if (comment) *comment = '\0';

    // Tokenize
    char *tokens[8];
    int token_count = 0;
    char *token = strtok(line, " \t\n\r,");
    while (token && token_count < 8) {
        tokens[token_count++] = token;
        token = strtok(NULL, " \t\n\r,");
    }
    if (token_count == 0) return;

    // Directives
    if (strcasecmp(tokens[0], ".ORG") == 0) {
        if (token_count < 2) { print_error(".ORG requires an address"); return; }
        uint32_t addr = parse_number(tokens[1]);
        int found = 0;
        for (int i = 0; i < NUM_REGIONS; i++) {
            if (addr >= REGIONS[i].start && addr <= REGIONS[i].end) {
                found = 1;
                if (REGIONS[i].is_protected) {
                    print_error("Cannot assemble into protected region");
                    return;
                }
                current_org = addr;
                current_region = &REGIONS[i];
                output_offset = addr;
                break;
            }
        }
        if (!found) print_error(".ORG address not in valid region");
        return;
    }
    if (strcasecmp(tokens[0], ".BYTE") == 0) {
        for (int i = 1; i < token_count; i++) {
            emit_byte((uint8_t)resolve_or_fixup(tokens[i], 0xFF, 1, 0xFF));
        }
        return;
    }

    // Labels
    if (tokens[0][strlen(tokens[0]) - 1] == ':') {
        tokens[0][strlen(tokens[0]) - 1] = '\0';
        if (find_label(tokens[0]) != 0xFFFFFFFF) {
            print_error("Duplicate label");
        } else {
            define_label(tokens[0], output_offset);
        }
        // Shift tokens left
        for (int i = 1; i < token_count; i++) tokens[i - 1] = tokens[i];
        token_count--;
        if (token_count == 0) return;
    }

    // Instructions
    Instruction *instr = lookup_instruction(tokens[0]);
    if (!instr) {
        print_error("Unknown mnemonic");
        return;
    }

    if (token_count - 1 != instr->num_ops) {
        print_error("Incorrect operand count");
        return;
    }

    char **ops = &tokens[1];

    if (strcmp(instr->format, "imm24_reg") == 0) {
        uint8_t ra = parse_register(ops[0]);
        emit_byte(instr->opcode | ra);
        uint32_t imm = resolve_or_fixup(ops[1], 0xFFFFFF, 3, 0xFFFFFF);
        emit_byte((imm >> 16) & 0xFF);
        emit_byte((imm >> 8) & 0xFF);
        emit_byte(imm & 0xFF);
    }
    else if (strcmp(instr->format, "imm24") == 0) {
        emit_byte(instr->opcode);
        uint32_t imm = resolve_or_fixup(ops[0], 0xFFFFFF, 3, 0xFFFFFF);
        emit_byte((imm >> 16) & 0xFF);
        emit_byte((imm >> 8) & 0xFF);
        emit_byte(imm & 0xFF);
    }
    else if (strcmp(instr->format, "reg3") == 0) {
        uint8_t ra = parse_register(ops[0]);
        uint8_t rb = parse_register(ops[1]);
        uint8_t rc = parse_register(ops[2]);
        emit_byte(instr->opcode | ra);
        emit_byte((rb << 4) | rc);
    }
    else if (strcmp(instr->format, "reg1") == 0) {
        uint8_t ra = parse_register(ops[0]);
        emit_byte(instr->opcode | ra);
        emit_byte(0x00);
    }
    else if (strcmp(instr->format, "reg0") == 0) {
        emit_byte(instr->opcode);
        emit_byte(0x00);
    }
    else if (strcmp(instr->format, "shift") == 0) {
        uint8_t ra = parse_register(ops[0]);
        emit_byte(instr->opcode | ra);
        if (is_register(ops[1])) {
            emit_byte((1 << 7) | (parse_register(ops[1]) & 0x3F));
        } else {
            uint32_t val = resolve_or_fixup(ops[1], 0x3F, 1, 0x3F);
            emit_byte((0 << 7) | (val & 0x3F));
        }
    }
    else if (strcmp(instr->format, "imm3") == 0) {
        uint8_t ra = parse_register(ops[0]);
        emit_byte(instr->opcode | ra);
        uint32_t imm = resolve_or_fixup(ops[1], 0x07, 1, 0x07);
        emit_byte(imm & 0x07);
    }
    else if (strcmp(instr->format, "sub_2reg") == 0) {
        uint8_t ra = parse_register(ops[0]);
        uint8_t rb = parse_register(ops[1]);
        emit_byte(instr->opcode);
        emit_byte((ra << 4) | rb);
    }
    else if (strcmp(instr->format, "sub_1reg") == 0) {
        uint8_t ra = parse_register(ops[0]);
        emit_byte(instr->opcode);
        emit_byte(ra << 4);
    }
    else if (strcmp(instr->format, "sub_jmpr") == 0) {
        uint8_t ra = parse_register(ops[0]);
        emit_byte(instr->opcode);
        emit_byte((ra << 4) | instr->flag);
    }
    else if (strcmp(instr->format, "sub_0reg") == 0) {
        emit_byte(instr->opcode);
        emit_byte(0x00);
    }
}

void apply_fixups() {
    for (int i = 0; i < fixup_count; i++) {
        uint32_t val = find_label(fixups[i].label);
        if (val == 0xFFFFFFFF) {
            int prev_line = current_line_number; // [NEW] Save current line
            current_line_number = fixups[i].line_num; // [NEW] Set to fixup's line
            print_error("Undefined symbol during fixup");
            current_line_number = prev_line; // [NEW] Restore current line
            continue;
        }
        if (val > fixups[i].mask) {
            int prev_line = current_line_number; // [NEW]
            current_line_number = fixups[i].line_num; // [NEW]
            print_error("Fixup exceeds field limit");
            current_line_number = prev_line; // [NEW]
        }
        for (int b = 0; b < fixups[i].num_bytes; b++) {
            int shift = (fixups[i].num_bytes - 1 - b) * 8;
            output_buffer[fixups[i].offset + b] = (val >> shift) & 0xFF;
        }
    }
}

// --- MAIN ENTRY POINT ---
int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <input.asm> <output.bin> [size_flag]\n", argv[0]);
        printf("Max binary size flags:\n");
        printf("     -rom   : 8KB  (0x02000) - Standard ROM\n");
        printf("     -32    : 32KB (0x08000) - Single Persistent Memory bank\n");
        printf("     -full  : 64KB (0x10000) - Full Work RAM / Max Standard\n");
        printf("     -16    : 16KB (0x04000) - Default\n");
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    const char *flag = (argc >= 4) ? argv[3] : NULL;

    if (flag) {
        if (strcmp(flag, "-rom") == 0) {
            output_buffer_size = 0x02000;
        } else if (strcmp(flag, "-32") == 0) {
            output_buffer_size = 0x08000;
        } else if (strcmp(flag, "-full") == 0) {
            output_buffer_size = 0x10000;
        } else if (strcmp(flag, "-16") == 0) {
            output_buffer_size = 0x04000;
        } else {
            fprintf(stderr, "Unknown size flag '%s'. Use -rom, -16, -32, or -full.\n", flag);
            return 1;
        }
    } else {
        output_buffer_size = 0x04000; // Default 16KB
    }

    FILE *f_in = fopen(input_path, "r");
    if (!f_in) {
        perror("Could not open input file");
        return 1;
    }

    output_buffer = calloc(output_buffer_size, 1);
    if (!output_buffer) {
        perror("Failed to allocate output buffer");
        fclose(f_in);
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    current_line_number = 1; // [NEW] Start at line 1
    while (fgets(line, sizeof(line), f_in)) {
        assemble_line(line);
        current_line_number++; // [NEW] Increment for the next line
    }
    fclose(f_in);

    apply_fixups();

    if (error_flag) {
        printf("Assembly failed with errors.\n");
        free(output_buffer);
        return 1;
    }

    FILE *f_out = fopen(output_path, "wb");
    if (!f_out) {
        perror("Could not open output file");
        free(output_buffer);
        return 1;
    }

    fwrite(output_buffer, 1, output_offset, f_out);
    fclose(f_out);

    printf("Assembly successful! Wrote %u bytes (max %u) to %s\n", 
           output_offset, output_buffer_size, output_path);
    free(output_buffer);
    return 0;
}