#include "ncc.h"

/* ============================================================
 * Code Generator Initialization
 * ============================================================ */

static Section *section_new(CompilerState *cs, const char *name, int type, int flags)
{
    Section *sec;

    sec = nihao_malloc(cs, sizeof(Section));
    memset(sec, 0, sizeof(Section));

    strncpy(sec->name, name, sizeof(sec->name) - 1);
    sec->sh_type = type;
    sec->sh_flags = flags;
    sec->sh_addr = 0;
    sec->sh_addralign = 16;

    sec->data = NULL;
    sec->data_allocated = 0;
    sec->data_size = 0;

    return sec;
}

static void codegen_ensure_space(Section *sec, int needed)
{
    if (sec->data_size + needed > sec->data_allocated) {
        int new_alloc = sec->data_allocated ? sec->data_allocated * 2 : 4096;
        while (new_alloc < sec->data_size + needed) {
            new_alloc *= 2;
        }
        sec->data = realloc(sec->data, new_alloc);
        if (!sec->data) {
            fprintf(stderr, "Fatal: codegen buffer allocation failed\n");
            exit(1);
        }
        sec->data_allocated = new_alloc;
    }
}

static void codegen_emit_byte(Section *sec, unsigned char b)
{
    codegen_ensure_space(sec, 1);
    sec->data[sec->data_size++] = b;
}

static void codegen_emit_int32(Section *sec, int32_t val)
{
    codegen_ensure_space(sec, 4);
    sec->data[sec->data_size++] = val & 0xff;
    sec->data[sec->data_size++] = (val >> 8) & 0xff;
    sec->data[sec->data_size++] = (val >> 16) & 0xff;
    sec->data[sec->data_size++] = (val >> 24) & 0xff;
}

void codegen_init(CompilerState *cs)
{
    CodeGenState *cg = &cs->codegen;

    memset(cg, 0, sizeof(CodeGenState));
    cg->cs = cs;

    /* Create standard sections */
    cg->text_section = section_new(cs, ".text", 1, 6);   /* SHT_PROGBITS | SHF_ALLOC|SHF_EXECINSTR */
    cg->data_section = section_new(cs, ".data", 1, 3);   /* SHF_ALLOC|SHF_WRITE */
    cg->bss_section = section_new(cs, ".bss", 8, 3);     /* SHT_NOBITS */
    cg->rodata_section = section_new(cs, ".rodata", 1, 2); /* SHF_ALLOC */

    cg->cur_text_section = cg->text_section;
    cg->ind = 0;
    cg->loc = 0;

    /* Value stack */
    cg->vstack_size = 256;
    cg->vstack = nihao_malloc(cs, sizeof(SValue) * cg->vstack_size);
    cg->vtop = 0;

    /* Register allocator */
    cg->reg_count = 8;
    memset(cg->reg_alloc, 0, sizeof(cg->reg_alloc));

    /* Relocations */
    cg->reloc_capacity = 256;
    cg->relocs = nihao_malloc(cs, sizeof(int) * cg->reloc_capacity);
    cg->reloc_count = 0;

    cg->last_line_num = 0;
    cg->last_ind = 0;

    if (cs->verbose) {
        printf("Code generator initialized\n");
    }
}

/* ============================================================
 * Code Optimization (Placeholder)
 * ============================================================ */

void codegen_optimize(CompilerState *cs)
{
    /* Placeholder for peephole optimization, dead code elimination,
     * register allocation improvements, etc.
     *
     * A full implementation would:
     * - Scan the generated code for redundant instructions
     * - Eliminate unnecessary push/pop pairs
     * - Optimize constant expressions
     * - Perform register coalescing
     *
     * For now this is a no-op stub.
     */

    if (cs->verbose) {
        printf("Optimizing generated code...\n");
        printf("  .text size: %d bytes\n", cs->codegen.text_section->data_size);
        printf("  .data size: %d bytes\n", cs->codegen.data_section->data_size);
        printf("  .rodata size: %d bytes\n", cs->codegen.rodata_section->data_size);
    }
}

/* ============================================================
 * Function Prologue / Epilogue (Simple versions)
 * ============================================================ */

/* Simple x86-64 function prologue:
 *   push rbp
 *   mov rbp, rsp
 *   sub rsp, N   (allocate stack space for locals)
 */
void gen_function_prologue(Symbol *sym)
{
    CompilerState *cs = g_cs;
    Section *sec = cs->codegen.cur_text_section;
    int stack_size;

    if (!sym) return;

    /* Calculate stack frame size (simple: locals * 16 bytes) */
    stack_size = (sym->local_count + 1) * 16;
    stack_size = (stack_size + 15) & ~15; /* align to 16 bytes */

    /* push rbp */
    codegen_emit_byte(sec, 0x55);

    /* mov rbp, rsp */
    codegen_emit_byte(sec, 0x48);
    codegen_emit_byte(sec, 0x89);
    codegen_emit_byte(sec, 0xe5);

    /* sub rsp, imm32 */
    if (stack_size > 0) {
        codegen_emit_byte(sec, 0x48);
        codegen_emit_byte(sec, 0x83);
        codegen_emit_byte(sec, 0xec);
        codegen_emit_byte(sec, stack_size & 0xff);
    }

    cs->codegen.ind = sec->data_size;
}

void gen_function_epilogue(Symbol *sym)
{
    CompilerState *cs = g_cs;
    Section *sec = cs->codegen.cur_text_section;

    (void)sym;

    /* leave */
    codegen_emit_byte(sec, 0xc9);

    /* ret */
    codegen_emit_byte(sec, 0xc3);

    cs->codegen.ind = sec->data_size;
}

/* ============================================================
 * Function Prologue / Epilogue (Full versions)
 *
 * Full versions include callee-saved register saving,
 * proper stack frame setup, and frame pointer management.
 * ============================================================ */

void gen_function_prologue_full(Symbol *func_sym)
{
    CompilerState *cs = g_cs;
    Section *sec = cs->codegen.cur_text_section;
    int stack_size;
    int saved_regs = 4; /* rbx, r12, r13, r14, r15 simplified to 4 */

    if (!func_sym) return;

    /* Calculate total stack frame */
    stack_size = (func_sym->local_count * 8) + (saved_regs * 8) + 8;
    stack_size = (stack_size + 15) & ~15; /* 16-byte alignment */

    /* Standard prologue */
    /* push rbp */
    codegen_emit_byte(sec, 0x55);
    /* mov rbp, rsp */
    codegen_emit_byte(sec, 0x48);
    codegen_emit_byte(sec, 0x89);
    codegen_emit_byte(sec, 0xe5);

    /* Allocate stack frame */
    if (stack_size <= 127) {
        /* sub rsp, imm8 */
        codegen_emit_byte(sec, 0x48);
        codegen_emit_byte(sec, 0x83);
        codegen_emit_byte(sec, 0xec);
        codegen_emit_byte(sec, stack_size & 0xff);
    } else {
        /* sub rsp, imm32 */
        codegen_emit_byte(sec, 0x48);
        codegen_emit_byte(sec, 0x81);
        codegen_emit_byte(sec, 0xec);
        codegen_emit_int32(sec, stack_size);
    }

    /* Save callee-saved registers (rbx, r12-r15 simplified) */
    /* push rbx */
    codegen_emit_byte(sec, 0x53);
    /* push r12 */
    codegen_emit_byte(sec, 0x41);
    codegen_emit_byte(sec, 0x54);
    /* push r13 */
    codegen_emit_byte(sec, 0x41);
    codegen_emit_byte(sec, 0x55);

    cs->codegen.ind = sec->data_size;
}

void gen_function_epilogue_full(Symbol *func_sym)
{
    CompilerState *cs = g_cs;
    Section *sec = cs->codegen.cur_text_section;

    (void)func_sym;

    /* Restore callee-saved registers (reverse order) */
    /* pop r13 */
    codegen_emit_byte(sec, 0x41);
    codegen_emit_byte(sec, 0x5d);
    /* pop r12 */
    codegen_emit_byte(sec, 0x41);
    codegen_emit_byte(sec, 0x5c);
    /* pop rbx */
    codegen_emit_byte(sec, 0x5b);

    /* leave */
    codegen_emit_byte(sec, 0xc9);

    /* ret */
    codegen_emit_byte(sec, 0xc3);

    cs->codegen.ind = sec->data_size;
}

/* ============================================================
 * Control Flow: If Statement (Simple)
 * ============================================================ */

static int label_counter = 0;

static int new_label(void)
{
    return label_counter++;
}

void gen_if(void)
{
    /* Simple stub: the actual implementation would:
     * 1. Pop the condition value from the value stack
     * 2. Test if zero (false)
     * 3. Jump to else label or end label
     * 4. Emit then-branch code
     * 5. If else exists, jump over else to end label
     * 6. Emit else-branch code
     * 7. Place end label
     *
     * For now we just record that an if was generated.
     */
    CompilerState *cs = g_cs;
    (void)new_label();

    if (cs->verbose && cs->debug_mode) {
        printf("gen_if: stub (condition consumed)\n");
    }
}

void gen_if_statement(CompilerState *cs)
{
    /* Full if statement code generation.
     * Uses value stack for condition evaluation result.
     *
     * Structure:
     *   <condition code>
     *   test eax, eax
     *   jz .else_label
     *   <then block>
     *   jmp .end_label
     * .else_label:
     *   <else block>
     * .end_label:
     */
    Section *sec = cs->codegen.cur_text_section;
    int else_lbl = new_label();
    int end_lbl = new_label();

    /* TODO: Pop condition from value stack and generate test + jump
     * For now, this is a structural placeholder.
     */

    /* test eax, eax (assume condition result is in eax) */
    codegen_emit_byte(sec, 0x85);
    codegen_emit_byte(sec, 0xc0);

    /* jz rel32 (placeholder offset, will be patched later) */
    codegen_emit_byte(sec, 0x0f);
    codegen_emit_byte(sec, 0x84);
    codegen_emit_int32(sec, 0); /* placeholder: else_lbl offset */

    /* Then block code is emitted by the parser after this call */

    /* jmp rel32 to end (placeholder - would be emitted after then block) */
    /* codegen_emit_byte(sec, 0xe9);
       codegen_emit_int32(sec, 0); */

    /* The actual label patching happens in a second pass or
     * via a relocation system. This is a simplified stub. */

    if (cs->verbose && cs->debug_mode) {
        printf("gen_if_statement: labels else=%d end=%d\n", else_lbl, end_lbl);
    }
}

/* ============================================================
 * Control Flow: While Loop
 * ============================================================ */

void gen_while(void)
{
    /* Simple stub for while loop code generation.
     * Full implementation:
     * 1. Place condition label
     * 2. Evaluate condition
     * 3. Jump to end label if false
     * 4. Emit body
     * 5. Jump back to condition label
     * 6. Place end label
     */
    CompilerState *cs = g_cs;
    (void)new_label();

    if (cs->verbose && cs->debug_mode) {
        printf("gen_while: stub\n");
    }
}

void gen_while_loop(CompilerState *cs)
{
    Section *sec = cs->codegen.cur_text_section;
    int cond_lbl = new_label();
    int end_lbl = new_label();

    /* Place condition label (would be recorded in label table) */
    /* TODO: Evaluate condition and jump to end if false */

    /* test eax, eax */
    codegen_emit_byte(sec, 0x85);
    codegen_emit_byte(sec, 0xc0);

    /* jz end_lbl */
    codegen_emit_byte(sec, 0x0f);
    codegen_emit_byte(sec, 0x84);
    codegen_emit_int32(sec, 0); /* placeholder */

    /* Body code emitted by parser... */

    /* jmp cond_lbl (emitted after body) */
    /* codegen_emit_byte(sec, 0xe9);
       codegen_emit_int32(sec, 0); */

    if (cs->verbose && cs->debug_mode) {
        printf("gen_while_loop: cond=%d end=%d\n", cond_lbl, end_lbl);
    }
}

/* ============================================================
 * Control Flow: For Loop
 * ============================================================ */

void gen_for(void)
{
    /* Simple stub for for loop.
     * Structure similar to while but with init and increment sections.
     */
    CompilerState *cs = g_cs;
    (void)new_label();

    if (cs->verbose && cs->debug_mode) {
        printf("gen_for: stub\n");
    }
}

void gen_for_loop(CompilerState *cs)
{
    Section *sec = cs->codegen.cur_text_section;
    int cond_lbl = new_label();
    int inc_lbl = new_label();
    int end_lbl = new_label();

    /* Init code already emitted before this call */

    /* Condition label */
    /* test condition, jump to end if false */
    codegen_emit_byte(sec, 0x85);
    codegen_emit_byte(sec, 0xc0);
    codegen_emit_byte(sec, 0x0f);
    codegen_emit_byte(sec, 0x84);
    codegen_emit_int32(sec, 0); /* placeholder: jump to end */

    /* Body code emitted by parser... */

    /* Increment label and code (emitted after body via continue jumps) */
    /* jmp cond_lbl at end of body */

    if (cs->verbose && cs->debug_mode) {
        printf("gen_for_loop: cond=%d inc=%d end=%d\n", cond_lbl, inc_lbl, end_lbl);
    }
}

/* ============================================================
 * Control Flow: Do-While Loop
 * ============================================================ */

void gen_do_while_loop(CompilerState *cs)
{
    Section *sec = cs->codegen.cur_text_section;
    int start_lbl = new_label();
    (void)start_lbl;

    /* Body is emitted first (before this call conceptually) */
    /* Then condition is evaluated at the bottom */
    /* If true, jump back to start */

    /* test eax, eax */
    codegen_emit_byte(sec, 0x85);
    codegen_emit_byte(sec, 0xc0);

    /* jnz start_lbl */
    codegen_emit_byte(sec, 0x0f);
    codegen_emit_byte(sec, 0x85);
    codegen_emit_int32(sec, 0); /* placeholder */

    if (cs->verbose && cs->debug_mode) {
        printf("gen_do_while_loop: start=%d\n", start_lbl);
    }
}

/* ============================================================
 * Return Statement
 * ============================================================ */

void gen_return(void)
{
    /* Simple stub for return statement.
     * Full implementation:
     * 1. If return value exists, move to return register (rax)
     * 2. Jump to function epilogue
     */
    CompilerState *cs = g_cs;
    Section *sec = cs->codegen.cur_text_section;

    /* ret (simplified - proper version jumps to epilogue) */
    codegen_emit_byte(sec, 0xc3);

    cs->codegen.ind = sec->data_size;
}

void gen_return_statement(CompilerState *cs)
{
    Section *sec = cs->codegen.cur_text_section;

    /* If there's a return value on the stack, pop it into rax
     * For now, simplified: just emit return sequence
     */

    /* mov rsp, rbp */
    codegen_emit_byte(sec, 0x48);
    codegen_emit_byte(sec, 0x89);
    codegen_emit_byte(sec, 0xec);

    /* pop rbp */
    codegen_emit_byte(sec, 0x5d);

    /* ret */
    codegen_emit_byte(sec, 0xc3);

    cs->codegen.ind = sec->data_size;

    if (cs->verbose && cs->debug_mode) {
        printf("gen_return_statement: emitted\n");
    }
}
