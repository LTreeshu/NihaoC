#ifndef NCC_H
#define NCC_H 

#ifdef __TINYC__
/* Windows tcc 的 libc 提供 _strtoi64 而非 strtoll；
 * Linux/macOS tcc 同样定义 __TINYC__ 但标准库有 strtoll，
 * 必须限定 _WIN32 否则 Linux tcc 会错误映射（undefined _strtoi64，2026-08-31 实测） */
#if defined(_WIN32)
#define strtoll _strtoi64
#endif
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>

#include "token.h"
/* ============================================================
 * Configuration & Limits
 * ============================================================ */

/* 版本单一真源是 xmake.lua 的 set_version，经 add_defines 以 NIHAO_VERSION_TAG 注入并字符串化。
   不经 xmake 构建（如 LEGACY Makefile）时无该宏，落到下方占位值，不代表任一对外发布口径。 */
#define NIHAO_STR_(x) #x
#define NIHAO_STR(x)  NIHAO_STR_(x)
#ifdef NIHAO_VERSION_TAG
#define NIHAO_VERSION   NIHAO_STR(NIHAO_VERSION_TAG)
#else
#define NIHAO_VERSION   "0.0.0-unknown"
#endif
#define TOK_HASH_SIZE       2048
#define TOK_MAX_SIZE        128
#define MAX_NESTING_DEPTH   256
#define MAX_SYMBOLS         65536
#define MAX_MODULES         256
#define MAX_LINK_LIBS       64
#define MAX_MULTIRETURN     16
#define MAX_INIT_DESIGATORS 32
/* 成员默认值展开的缓冲上限（§3.1 × §5.1.2：聚合类型数组按元素逐项展开）；
 * 超出时前端即时报错，不生成截断的初始化器 */
#define DEFAULT_INIT_BUF 8192

#define match(a,b)  (strcmp(a,b) == 0)

/* ============================================================
 * Type System
 * ============================================================ */

/* Base type kinds */
typedef enum {
    TYPE_NONE = 0,
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_F32, TYPE_F64,
    TYPE_FX32, TYPE_FX64,
    TYPE_BOOL,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNC,
    TYPE_ALIAS,
} TypeKind;

/* Visibility attributes
 * 与 cgen 端 enum nihao_vis 数值对齐（cgen.c:270：NH_UNDEF=0, NH_CONST, NH_FLOW, NH_STATIC, NH_VAR）：
 *   VIS_UNDEF=0  VIS_CONST=1  VIS_FLOW=2  VIS_STATIC=3  VIS_VAR=4
 * PB-27.9 补 VIS_VAR（替换原 VIS_DEFAULT=0 的语义重复），PB-27.3 统一数值。 */
typedef enum {
    VIS_UNDEF = 0,      /* undefined visibility */
    VIS_CONST,          /* read-only global */
    VIS_FLOW,           /* dynamically tracked heap */
    VIS_STATIC,         /* static lifetime */
    VIS_VAR             /* default "new value" semantics（无前缀声明 / 默认返回） */
} Visibility;


/* ============================================================
 * 以下 CType / Symbol / SymKind / TokenSym / Module / LinkLib 及
 * ParserState、CompilerState 中引用它们字段，均为 2026-11-12 已物理删除的
 * A 线的遗留类型（IR 线不再使用）。extern 函数声明与 -run 字段已清理；
 * 这些类型定义因与两大状态结构体互相引用、删除需连带重写结构体，风险大于
 * 收益，暂保留并在此标注为 dead-code 遗留，待后续专项清理。
 * ============================================================ */

/* Forward declarations */
typedef struct CType CType;
typedef struct Symbol Symbol;
typedef struct TokenSym TokenSym;
typedef struct CompilerState CompilerState;

/* Type structure */
struct CType {
    TypeKind kind;
    Visibility vis;
    unsigned int size;          /* size in bytes */
    unsigned int align;         /* alignment */
    unsigned int bit_size;      /* for bitfields: 0 = not a bitfield */
    unsigned int bit_offset;    /* bitfield offset */
    CType *ref;                 /* for pointer/array/alias: referenced type */
    Symbol *sym;                /* for struct/union/enum: symbol reference */
    CType *next;                /* for function: return type */
    CType *params;              /* for function: parameter types */
    int param_count;
    CType *return_types[MAX_MULTIRETURN];
    int return_count;
};

/* ============================================================
 * Symbol Table
 * ============================================================ */

typedef enum {
    SYM_NONE = 0,
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_STRUCT,
    SYM_UNION,
    SYM_ENUM,
    SYM_TYPEDEF,        /* alias */
    SYM_MODULE,
    SYM_LABEL,
} SymKind;

struct Symbol {
    SymKind kind;
    char *name;
    int hash;
    CType *type;
    Visibility vis;
    int ret_vis;          /* 仅 SYM_FUNCTION：返回值可见性前缀（PB-27）；VIS_VAR=4 默认 / VIS_CONST=1 / VIS_FLOW=2 / VIS_STATIC=3 */

    /* Storage info */
    int is_defined;             /* has body been defined */
    int is_extern;              /* external symbol */
    int is_builtin;             /* built-in function */
    int ownership_transferred;  /* flow ptr returned: skip auto-free */
    /* flow 变量的初值（或最后一次整变量赋值）不是堆所有权：字符串字面量在静态只读段、
       `&x` 指向栈帧或他方对象，两者在块/函数退出时 free() 都是非法释放（§11.1） */
    int no_auto_free;

    /* Ownership/borrow state (NihaoC ch.12): */
    /* 0 = valid, 1 = frozen (borrowed), 2 = invalid (ownership moved) */
    int borrow_state;
    Symbol *borrow_source;      /* who this var borrows from (for unfreeze) */
    /* 指针当前所指对象的字节数，`.()` 越界检查用；0 = 静态未知（不检查） */
    unsigned int pointee_bytes;
    /* 通用 `void` 槽位在声明处由 `malloc(T)` / `&x` 定下的所指类型，供省略类型的
       裸 `.()` / `p[]` / `->` 还原成 `(*(T*)(p))`；NULL = 静态未知 */
    CType *pointee_type;
    /* len(x) 内置函数的逻辑长度：数组=元素个数、动态字符串 char[]=字面量长度、
       切片变量=边界差 hi-lo；len_known = 0 表示静态不可知（len() 报错） */
    int len_known;
    long long logical_len;
    
    /* Location in source */
    char *filename;
    int line_num;
    
    /* Code generation data */
    int stack_offset;           /* offset in stack frame */
    unsigned long addr;         /* address in code section */
    
    /* For functions */
    Symbol *params;             /* parameter list */
    Symbol *locals;             /* local variable list */
    int local_count;
    
    /* For struct/union */
    Symbol *members;
    int member_count;
    int total_size;
    int total_align;
    /* 成员默认值（`x i32 = 7`）的 C 初值文本，仅成员 Symbol 用；NULL = 无默认值 */
    char *def_init;
    
    /* For modules */
    Symbol *module_symbols;
    char *module_name;
    int symbol_count;
    
    /* Hash table chain */
    Symbol *hash_next;
    Symbol *next;               /* next in same scope */
};

/* Token symbol (identifier hash table entry) */
struct TokenSym {
    char *str;
    int len;
    int tok;                    /* TOK_IDENT or keyword token */
    TokenSym *hash_next;
};

/* ============================================================
 * Lexer State
 * ============================================================ */

typedef struct {
    char *filename;
    char *buffer;               /* input buffer */
    char *buf_ptr;              /* current position */
    char *buf_end;              /* end of buffer */
    int line_num;
    int col_num;
    int last_line_num;
    
    /* Token buffer */
    TokenType tok;
    union {
        int64_t i;
        uint64_t u;
        double f;
        char *str;
    } tok_val;
    char *tok_str;              /* string value of current token */
    int tok_len;
    char ident_buf[256];        /* fixed buffer for identifier tokens
                                 * (peek-safe: never freed) */
    
    /* Lookahead */
    int peek_tok;
    int peek_valid;
    char *peek_str;             /* string value of peeked token (strdup, permanent) */
    
    /* Preprocessor state */
    int in_comment;
    int in_string;
    int paren_depth;
    int brace_depth;
    int bracket_depth;
} LexerState;

/* ============================================================
 * Parser State
 * ============================================================ */

typedef struct {
    CompilerState *cs;
    LexerState *lex;
    TokenSym **table_ident;
    TokenSym *hash_ident[TOK_HASH_SIZE];
    
    /* Current context */
    Symbol *cur_module;
    Symbol *cur_func;
    Symbol *cur_struct;
    Symbol *last_ident;         /* last identifier referenced in an expr */
    /* 刚解析的后缀链是裸标识符（无任何后缀步），赋值即整变量重绑定 */
    int lhs_bare_ident;
    /* 本语句自身转移出去的 flow 源：右值取值仍须放行，语句结束即失效（§12.1） */
    Symbol *moved_src;
    int scope_depth;
    
    /* Parsing flags */
    int parse_flags;
    int const_wanted;
    int nocode_wanted;
    
    /* Expression parsing */
    int *macro_ptr;
    int unget_buffer_enabled;
    unsigned int malloc_bytes;  /* 本次 malloc(T) 请求的字节数，供声明初始化记录 pointee_bytes */
    CType *init_pointee;        /* 本次初值静态已知的所指类型（malloc(T) / &x），供声明记录 pointee_type */
    int lhs_was_deref;          /* 赋值左侧是解引用链（`p.(T) = v`）而非对 p 本身赋值 */
    int rhs_was_slice;          /* 刚解析的表达式以切片读 `[a..b]` 结尾（数组声明据此走复制） */
    int slice_lmark;            /* 该切片读文本在 cgen 缓冲里的起点，用于确认它是赋值左侧整体 */
    int slice_len_known;        /* 最近一次切片读的上下界是否都是字面量（`len(切片变量)` 据此求值） */
    long long slice_len;        /* 该切片的逻辑长度 hi-lo */
    
    /* Error handling */
    int error_count;
    int warning_count;
    char error_msg[256];
} ParserState;

/* ============================================================
 * Module System
 * ============================================================ */

typedef struct {
    char *name;
    char *filename;
    int is_external;
    int visited;            /* parsed at least once (cycle guard) */
    Symbol *symbols;
    int symbol_count;
} Module;

typedef struct {
    char *name;
    char *alias;
    char *path;
    int is_static;
} LinkLib;

/* ============================================================
 * Compiler State (Global)
 * ============================================================ */

struct CompilerState {
    /* Configuration */
    int verbose;
    int debug_mode;
    int test_mode;              /* -lexertest: dump token stream only */
    int output_type;            /* 0=exec, 1=object, 2=shared, 3=static */
    int backend;                /* 2=ir-c (default) 3=ir-native 4=ir-riscv64 5=ir-loongarch64 */
    char *output_file;
    char *input_file;
    int argc;
    char **argv;
    
    /* Modules */
    Module modules[MAX_MODULES];
    int module_count;
    Module *current_module;
    
    /* Link libraries */
    LinkLib link_libs[MAX_LINK_LIBS];
    int link_lib_count;
    
    /* Parser state */
    ParserState parser;
    int while_depth;              /* >0 表示处于 while 循环体内，`is` 仅此上下文合法 */
    
    /* Symbol tables */
    TokenSym **table_ident;
    TokenSym *hash_ident[TOK_HASH_SIZE];
    Symbol *global_syms;
    Symbol *local_syms;
    
    /* Memory management */
    void **allocated_ptrs;
    int alloc_count;
    int alloc_capacity;
    
    /* String table */
    char **string_table;
    int string_count;
    int string_capacity;
    
    /* Output file */
    FILE *outfile;
    
    /* Error reporting */
    int error_count;
    int warning_count;
};

/* ============================================================
 * Function Declarations
 * ============================================================ */

/* ncc.c - Main program */
int  nihao_main(int argc, char **argv);
void nihao_error(CompilerState *cs, const char *fmt, ...);
void nihao_warning(CompilerState *cs, const char *fmt, ...);
void *nihao_malloc(CompilerState *cs, size_t size);
void *nihao_realloc(CompilerState *cs, void *ptr, size_t size);
char *nihao_strdup(CompilerState *cs, const char *str);
char *load_source_file(const char *filename, size_t *size_out);

/* lexer.c */
void lexer_init(CompilerState *cs, const char *filename, const char *source);
void lexer_next(LexerState *lex);
void lexer_peek(LexerState *lex);
const char *token_name(TokenType tok);
int is_keyword(const char *str, int len);
int is_type_token(TokenType tok);
int is_visibility_token(TokenType tok);

/* token 访问器（实现在 lexer.c——原在 parser.c，2026-11-12 A 线删除后移来） */
TokenType cur_tok(CompilerState *cs);
void next_tok(CompilerState *cs);
void expect(CompilerState *cs, TokenType tok);
void skip_newlines(CompilerState *cs);

/* irparse.c / ir.c / ir_to_c.c / ir_backend.c + ir_<arch>.c - IR middle layer (backend=ir-*) */
int ir_compile(CompilerState *cs, const char *filename, int backend, int verbose);

extern CompilerState *g_cs;

#endif /* NCC_H */
