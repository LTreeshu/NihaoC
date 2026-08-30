#include "ncc.h"

/* ============================================================
 * CGen: C backend (transpile NihaoC -> C, then invoke tcc)
 *
 * One-pass design: the parser calls into cgen as it parses,
 * so no full AST is needed (TCC-style single-pass philosophy,
 * but the target language is C instead of machine code).
 * ============================================================ */

typedef struct {
    char *buf;
    int len;
    int cap;
    int indent;
    int at_line_start;
    char func_return_name[64];   /* current function C return type */
} CGen;

static CGen cg;

void cgen_init(void)
{
    cg.buf = NULL;
    cg.len = 0;
    cg.cap = 0;
    cg.indent = 0;
    cg.at_line_start = 1;
    cg.func_return_name[0] = '\0';
}

static void cgen_reserve(int extra)
{
    if (cg.len + extra + 1 > cg.cap) {
        int new_cap = cg.cap ? cg.cap * 2 : 4096;
        while (new_cap < cg.len + extra + 1) new_cap *= 2;
        cg.buf = realloc(cg.buf, new_cap);
        if (!cg.buf) { fprintf(stderr, "Fatal: cgen buffer alloc failed\n"); exit(1); }
        cg.cap = new_cap;
    }
}

/* Emit a raw chunk (no newline). Applies indentation if at line start. */
void cgen_raw(const char *fmt, ...)
{
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (cg.at_line_start) {
        cgen_reserve(cg.indent * 4);
        memset(cg.buf + cg.len, ' ', cg.indent * 4);
        cg.len += cg.indent * 4;
        cg.buf[cg.len] = '\0';
        cg.at_line_start = 0;
    }
    int n = (int)strlen(tmp);
    cgen_reserve(n);
    memcpy(cg.buf + cg.len, tmp, n);
    cg.len += n;
    cg.buf[cg.len] = '\0';
    cg.at_line_start = 0;
}

/* Emit a complete line: indentation + content + newline */
void cgen_line(const char *fmt, ...)
{
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (cg.at_line_start) {
        cgen_reserve(cg.indent * 4);
        memset(cg.buf + cg.len, ' ', cg.indent * 4);
        cg.len += cg.indent * 4;
        cg.buf[cg.len] = '\0';
    }
    int n = (int)strlen(tmp);
    cgen_reserve(n + 1);
    memcpy(cg.buf + cg.len, tmp, n);
    cg.len += n;
    cg.buf[cg.len++] = '\n';
    cg.buf[cg.len] = '\0';
    cg.at_line_start = 1;
}

/* Emit a blank line */
void cgen_blank(void)
{
    cgen_line("");
}

void cgen_indent(void) { cg.indent++; }
void cgen_dedent(void) { if (cg.indent > 0) cg.indent--; }

/* Get the accumulated C source */
const char *cgen_result(void)
{
    return cg.buf ? cg.buf : "";
}

/* 分段缓冲：cgen_mark 记录位置；cgen_slice 取 [mark,len) 文本（须立即使用）；
 * cgen_truncate 回退到 mark——多变量声明等"值先 emit 后重排"场景用 */
int cgen_mark(void)
{
    return cg.len;
}
const char *cgen_slice(int mark)
{
    static char slice_buf[4096];
    int n = cg.len - mark;
    if (n <= 0) return "";
    if (n >= (int)sizeof(slice_buf)) n = (int)sizeof(slice_buf) - 1;
    memcpy(slice_buf, cg.buf + mark, (size_t)n);
    slice_buf[n] = '\0';
    return slice_buf;
}
void cgen_truncate(int mark)
{
    if (mark >= 0 && mark <= cg.len) cg.len = mark;
}

/* ============================================================
 * Type name mapping: NihaoC CType -> C type string
 * Uses a small static buffer; caller must consume immediately.
 * ============================================================ */

/* 函数指针参数类型列表 → "int64_t, int64_t"（params 链表头插 → 收集反转）。
 * 供 c_type_name(TYPE_FUNC) 与函数指针变量/参数声明共用 */
void c_type_params(const CType *t, char *out, int sz)
{
    char plist[16][64];
    int pc = 0;
    if (sz <= 0) return;
    out[0] = '\0';
    if (!t) return;
    for (const CType *pp = t->params; pp && pc < 16; pp = pp->next) {
        const char *nm = c_type_name((CType *)pp);
        snprintf(plist[pc], sizeof(plist[pc]), "%s", nm);
        pc++;
    }
    for (int i = pc - 1; i >= 0; i--) {
        if (out[0]) strncat(out, ", ", (size_t)(sz - strlen(out) - 1));
        strncat(out, plist[i], (size_t)(sz - strlen(out) - 1));
    }
}

const char *c_type_name(CType *t)
{
    static char buf[256];
    char elem[128];

    if (!t) return "void";

    switch (t->kind) {
        case TYPE_VOID:    return "void*";       /* NihaoC void = generic pointer */
        case TYPE_CHAR:    return "char";
        case TYPE_STRING:  return "char*";
        case TYPE_BOOL:    return "bool";
        case TYPE_U8:      return "uint8_t";
        case TYPE_U16:     return "uint16_t";
        case TYPE_U32:     return "uint32_t";
        case TYPE_U64:     return "uint64_t";
        case TYPE_I8:      return "int8_t";
        case TYPE_I16:     return "int16_t";
        case TYPE_I32:     return "int32_t";
        case TYPE_I64:     return "int64_t";
        case TYPE_F32:     return "float";
        case TYPE_F64:     return "double";
        case TYPE_FX32:    return "int32_t";     /* Q16.16 fixed point */
        case TYPE_FX64:    return "int64_t";     /* Q32.32 fixed point */
        case TYPE_ENUM:
            if (t->sym && t->sym->name) {
                snprintf(buf, sizeof(buf), "%s", t->sym->name);
                return buf;
            }
            return "int";
        case TYPE_STRUCT:
        case TYPE_UNION:
            if (t->sym && t->sym->name) {
                snprintf(buf, sizeof(buf), "%s", t->sym->name);
                return buf;
            }
            return "struct __anon";
        case TYPE_ALIAS:
            if (t->ref) return c_type_name(t->ref);
            return "void";
        case TYPE_POINTER:
            /* 具名指针：Point* -> "Point*"；通用 void* -> "void*"；
             * 多层：Point** -> "Point**"（递归 ref 名 + 星号）
             * ⚠️ 递归 c_type_name 返回同一 static buf，直接作 snprintf 源会 src==dst
             * 重叠写（UB）：Windows CRT 恰好容忍，Linux glibc 实测损坏（ir_arrow
             * 生成 "* p = &pt" 而非 "Point* p = &pt"，2026-08-31 WSL 复现）。
             * 先拷贝到本地数组再格式化。 */
            if (t->ref && t->ref->kind != TYPE_VOID) {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", c_type_name(t->ref));
                snprintf(buf, sizeof(buf), "%s*", tmp);
                return buf;
            }
            return "void*";
        case TYPE_ARRAY:
            if (t->ref) {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", c_type_name(t->ref));
                const char *et = tmp;
                if (t->param_count > 0) {
                    /* Fixed-size array: name goes AFTER the array suffix in C,
                     * so return the element type here; use c_type_suffix() for
                     * the "[N]" part when emitting declarations. */
                    snprintf(buf, sizeof(buf), "%s", et);
                } else {
                    /* dynamic array: pointer to element type
                     * (void[] -> void**, void[][] -> void***) */
                    snprintf(buf, sizeof(buf), "%s*", et);
                }
                return buf;
            }
            return "void*";
        case TYPE_FUNC:
            /* Function pointer: void(args) ret -> ret(*)(args)。
             * 参数类型存在 params 链表（头插存储 → 先收集到本地数组再反转输出；
             * ⚠️ c_type_name 返回 static buf，须立即拷贝到本地槽） */
            {
                char tmp[256];
                const char *rt = t->next ? c_type_name(t->next) : "void";
                snprintf(tmp, sizeof(tmp), "%s", rt);   /* 独立拷贝，防 static buf 重叠 */
                char pstr[512];
                c_type_params(t, pstr, sizeof(pstr));
                snprintf(buf, sizeof(buf), "%s(*)(%s)", tmp,
                         pstr[0] ? pstr : "void");
                (void)elem;
                return buf;
            }
        default:
            return "void";
    }
}

/* Array suffix for C declarations: "[N]" for fixed arrays, "" otherwise */
const char *c_type_suffix(CType *t)
{
    static char buf[32];
    if (!t) return "";
    if (t->kind == TYPE_ARRAY && t->ref && t->param_count > 0) {
        snprintf(buf, sizeof(buf), "[%d]", t->param_count);
        return buf;
    }
    return "";
}

/* ============================================================
 * Header preamble
 * ============================================================ */

void cgen_header(void)
{
    cgen_line("/* Generated by ncc (NihaoC -> C transpiler) */");
    cgen_line("#include <stdio.h>");
    cgen_line("#include <stdlib.h>");
    cgen_line("#include <string.h>");
    cgen_line("#include <stdint.h>");
    cgen_line("#include <stdbool.h>");
    cgen_line("#include <stddef.h>");   /* offsetof 宏 */
    cgen_line("/* NihaoC visibility enum */");
    cgen_line("enum nihao_vis { NH_UNDEF=0, NH_CONST, NH_FLOW, NH_STATIC, NH_VAR };");
    cgen_blank();
}
