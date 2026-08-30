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

#define NIHAO_VERSION       "0.1.0"
#define TOK_HASH_SIZE       2048
#define TOK_MAX_SIZE        128
#define MAX_NESTING_DEPTH   256
#define MAX_SYMBOLS         65536
#define MAX_MODULES         256
#define MAX_LINK_LIBS       64
#define MAX_MULTIRETURN     16

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

/* Visibility attributes */
typedef enum {
    VIS_DEFAULT = 0,    /* local scope */
    VIS_CONST,          /* read-only global */
    VIS_FLOW,           /* dynamically tracked heap */
    VIS_STATIC,          /* static lifetime */
    VIS_UNDEF           /* undefined visibility */
} Visibility;


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
    
    /* Storage info */
    int is_defined;             /* has body been defined */
    int is_extern;              /* external symbol */
    int is_builtin;             /* built-in function */
    int ownership_transferred;  /* flow ptr returned: skip auto-free */

    /* Ownership/borrow state (NihaoC ch.12): */
    /* 0 = valid, 1 = frozen (borrowed), 2 = invalid (ownership moved) */
    int borrow_state;
    Symbol *borrow_source;      /* who this var borrows from (for unfreeze) */
    
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
    int scope_depth;
    
    /* Parsing flags */
    int parse_flags;
    int const_wanted;
    int nocode_wanted;
    
    /* Expression parsing */
    int *macro_ptr;
    int unget_buffer_enabled;
    
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
    int backend;                /* 0=c (default, external tcc), 1=native (libtcc in-process) */
    int run_mode;               /* -run: with native backend, compile to memory and execute */
    int run_argc;               /* -run 透传的程序参数 */
    char **run_argv;
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

/* cgen.c - C backend */
void cgen_init(void);
void cgen_raw(const char *fmt, ...);
void cgen_line(const char *fmt, ...);
void cgen_blank(void);
void cgen_indent(void);
void cgen_dedent(void);
const char *cgen_result(void);
int cgen_mark(void);                /* 分段缓冲：mark/slice/truncate */
const char *cgen_slice(int mark);
void cgen_truncate(int mark);
const char *c_type_name(CType *t);
const char *c_type_suffix(CType *t);
void c_type_params(const CType *t, char *out, int sz);   /* 函数指针参数列表 */
void cgen_header(void);

/* native.c - libtcc machine-code backend (-backend=native) */
int native_backend_available(void);
int native_memory_available(void);
int native_compile_string(const char *csrc, const char *outfile, int verbose, int debug);
int native_run_string(const char *csrc, int argc, char **argv, int verbose, int debug);

/* parser.c */
void parser_init(CompilerState *cs);
TokenType cur_tok(CompilerState *cs);
void next_tok(CompilerState *cs);
void expect(CompilerState *cs, TokenType tok);
void skip_newlines(CompilerState *cs);
void parse_type(CompilerState *cs, CType *type);
void parse_module(CompilerState *cs);
void parse_declaration(CompilerState *cs);
void parse_function(CompilerState *cs, Symbol *func_sym);
void parse_statement(CompilerState *cs);
void parse_expression(CompilerState *cs);

/* irparse.c / ir.c / ir_to_c.c / ir_to_native.c - IR middle layer (backend=ir-*) */
int ir_compile(CompilerState *cs, const char *filename, int backend, int verbose);

/* linker.c */
void linker_init(CompilerState *cs);
void link_add_library(CompilerState *cs, char *path, char *alias, char *lib_path);

/* sym.c */
Symbol *sym_find(CompilerState *cs, const char *tok_str);
Symbol *sym_push(CompilerState *cs, SymKind kind, const char *tok_str, CType *type);
Symbol *sym_add_member(CompilerState *cs, Symbol *sym, const char *name, CType *type);
Symbol *sym_push_local(CompilerState *cs, Symbol *func_sym, const char *name, CType *type);
Symbol *sym_pop_locals(CompilerState *cs, Symbol *func_sym);
Symbol *sym_register_builtins(CompilerState *cs);

/* type.c */
CType *type_new(CompilerState *cs, TypeKind kind);
CType *type_array(CompilerState *cs, void *elem_type, int size);

/* vis.c */
void visibility_init(CompilerState *cs);
void vis_scope_enter(CompilerState *cs);
void vis_scope_exit(CompilerState *cs);
int vis_is_pointer_type(CType *t);
int vis_check_transfer(Visibility src, Visibility dst);
void vis_update_source(Visibility src, Visibility dst, Symbol *src_sym);
int vis_check_usable(CompilerState *cs, Symbol *s);
int vis_check_writable(CompilerState *cs, Symbol *s);
void vis_check_assign(CompilerState *cs, Visibility dst_vis, Symbol *src_sym,
                      Symbol *dst_sym, const char *dst_name);
void vis_unfreeze_borrows(Symbol *borrow_head, Symbol *scope_start);

/* module.c */
Module *module_add(CompilerState *cs, const char *tok_str, char *input_file);
Module *module_import(CompilerState *cs, const char *name);

/* stdlib.c */
void stdlib_register_all(CompilerState *cs);
void stdlib_resolve_link_libraries(CompilerState *cs);
void stdlib_generate_runtime_stubs(CompilerState *cs);

extern CompilerState *g_cs;

#endif /* NCC_H */
