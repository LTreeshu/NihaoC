#ifndef NCC_H
#define NCC_H 

#ifdef __TINYC__
#define strtoll _strtoi64
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
    int is_multireturn;         /* multiple return values flag */
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
    
    /* Lookahead */
    int peek_tok;
    int peek_valid;
    
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
 * Code Generator State
 * ============================================================ */

/* Value on the virtual stack */
typedef struct {
    CType *type;
    unsigned short r;           /* register or VT_CONST */
    union {
        int64_t i;
        double f;
        void *ptr;
    } val;
    int sym;                    /* symbol index */
} SValue;

/* Code section */
typedef struct {
    unsigned char *data;
    int data_allocated;
    int data_size;
    int sh_num;                 /* section number */
    char name[32];
    int sh_type;
    int sh_flags;
    int sh_addr;
    int sh_addralign;
} Section;

typedef struct {
    CompilerState *cs;
    
    /* Output sections */
    Section *text_section;
    Section *data_section;
    Section *bss_section;
    Section *rodata_section;
    
    /* Current code generation state */
    int ind;                    /* output code index */
    int loc;                    /* local variable index */
    Section *cur_text_section;
    
    /* Value stack */
    SValue *vstack;
    int vstack_size;
    int vtop;                   /* top of value stack */
    
    /* Register allocation */
    int reg_count;
    int reg_alloc[8];           /* simple register allocator */
    
    /* Relocations */
    int *relocs;
    int reloc_count;
    int reloc_capacity;
    
    /* Debug info */
    int last_line_num;
    int last_ind;
} CodeGenState;

/* ============================================================
 * Module System
 * ============================================================ */

typedef struct {
    char *name;
    char *filename;
    int is_external;
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
    int output_type;            /* 0=exec, 1=object, 2=shared, 3=static */
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
    
    /* Code generator state */
    CodeGenState codegen;
    
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

extern CompilerState *g_cs;

#endif /* NCC_H */
