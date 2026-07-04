#ifndef _TOKEN_H
#define _TOKEN_H
/* token.h */

#define KeywordDefTable \
    KeywordDef("true",     4, TOK_TRUE)           /**< Boolean truth constant `true`  */   \
    KeywordDef("false",    5, TOK_FALSE)          /**< Boolean false value constant `false`  */   \
    /* Storage class specifier */ \
    KeywordDef("register", 8, TOK_REGISTER)       /**< Keyword `register`, hint to compiler to store variable in a register  */   \
    /* Type specifiers and qualifiers */ \
    KeywordDef("restrict", 8, TOK_RESTRICT)       /**< Keyword `restrict`, pointer qualifier for optimization  */   \
    KeywordDef("volatile", 8, TOK_VOLATILE)       /**< Keyword `volatile`, type qualifier indicating object may be changed unexpectedly  */   \
    /* Structure, union, enumeration */ \
    KeywordDef("struct",   6, TOK_STRUCT)         /**< Keyword `struct`, used to define structure types  */   \
    KeywordDef("union",    5, TOK_UNION)          /**< Keyword `union`, used to define union types  */   \
    KeywordDef("enum",     4, TOK_ENUM)           /**< Keyword `enum`, used to define enumeration types  */   \
    /* Control flow */ \
    KeywordDef("if",       2, TOK_IF)             /**< Keyword `if`, conditional statement  */   \
    KeywordDef("else",     4, TOK_ELSE)           /**< Keyword `else`, negative branch paired with `if`  */   \
    KeywordDef("switch",   6, TOK_SWITCH)         /**< Keyword `switch`, multi-way selection statement  */   \
    KeywordDef("case",     4, TOK_CASE)           /**< Keyword `case`, branch label in `switch` statement  */   \
    KeywordDef("default",  7, TOK_DEFAULT)        /**< Keyword `default`, default branch label in `switch` statement  */   \
    KeywordDef("for",      3, TOK_FOR)            /**< Keyword `for`, loop statement  */   \
    KeywordDef("do",       2, TOK_DO)             /**< Keyword `do`, part of `do...while` loop  */   \
    KeywordDef("while",    5, TOK_WHILE)          /**< Keyword `while`, loop statement or part of `do...while`  */   \
    KeywordDef("break",    5, TOK_BREAK)          /**< Keyword `break`, exit loop or `switch` statement  */   \
    KeywordDef("continue", 8, TOK_CONTINUE)       /**< Keyword `continue`, skip remaining part of current loop  */   \
    KeywordDef("goto",     4, TOK_GOTO)           /**< Keyword `goto`, unconditional jump statement  */   \
    KeywordDef("return",   6, TOK_RETURN)         /**< Keyword `return`, return from a function  */   \
    KeywordDef("const",    5, TOK_CONST)          /**< Keyword `const`, type qualifier indicating object is immutable  */   \
    KeywordDef("flow",     4, TOK_FLOW)           /**< Keyword `flow`, dynamically allocated variable declaration  */   \
    KeywordDef("static",   6, TOK_STATIC)         /**< Keyword `static`, specifies static storage duration or internal linkage  */   \
    KeywordDef("var",      3, TOK_VAR)            /**< Keyword `var`, local variable declaration  */   \
    KeywordDef("cooking",  7, TOK_COOKING)        /**< Keyword `cooking`, compile-time execution block  */   \
    KeywordDef("align",    5, TOK_ALIGN)          /**< Keyword `align`, byte alignment block  */   \
    KeywordDef("module",   6, TOK_MODULE)         /**< Keyword `module`, module definition  */   \
    KeywordDef("use",      3, TOK_USE)            /**< Keyword `use`, module import  */   \
    KeywordDef("alias",    5, TOK_ALIAS)          /**< Keyword `alias`, type alias  */   \
    KeywordDef("link",     4, TOK_LINK)           /**< Keyword `link`, used for static library export  */   \
    KeywordDef("sizeof",   6, TOK_SIZEOF)         /**< Keyword `sizeof`, get size of type or object  */   \
    KeywordDef("typeof",   6, TOK_TYPEOF)         /**< Keyword `typeof`, type inspection  */   \
    KeywordDef("alignof",  7, TOK_ALIGNOF)        /**< Keyword `alignof`, get alignment of type  */   \
    KeywordDef("offsetof", 8, TOK_OFFSETOF)       /**< Keyword `offsetof`, get offset of structure member  */   \
    KeywordDef("holdof",   6, TOK_HOLDOF)         /**< Keyword `holdof`, get base address of enclosing structure for a member  */   \
    KeywordDef("visof",    5, TOK_VISOF)          /**< Keyword `visof`, visibility check  */   \
    /* Type */ \
    KeywordDef("void",     4, TOK_VOID)           /**< Type `void`, generic pointer type  */   \
    KeywordDef("i8",       2, TOK_I8)             /**< Type `i8`, 8-bit signed integer  */   \
    KeywordDef("i16",      3, TOK_I16)            /**< Type `i16`, 16-bit signed integer  */   \
    KeywordDef("i32",      3, TOK_I32)            /**< Type `i32`, 32-bit signed integer  */   \
    KeywordDef("u8",       2, TOK_U8)             /**< Type `u8`, 8-bit unsigned integer  */   \
    KeywordDef("u16",      3, TOK_U16)            /**< Type `u16`, 16-bit unsigned integer  */   \
    KeywordDef("u32",      3, TOK_U32)            /**< Type `u32`, 32-bit unsigned integer  */   \
    KeywordDef("u64",      3, TOK_U64)            /**< Type `u64`, 64-bit unsigned integer  */   \
    KeywordDef("f32",      3, TOK_F32)            /**< Type `f32`, 32-bit floating point  */   \
    KeywordDef("f64",      3, TOK_F64)            /**< Type `f64`, 64-bit floating point  */   \
    KeywordDef("fx32",     4, TOK_FX32)           /**< Type `fx32`, 32-bit fixed-point  */   \
    KeywordDef("fx64",     4, TOK_FX64)           /**< Type `fx64`, 64-bit fixed-point  */   \
    KeywordDef("char",     4, TOK_CHAR)           /**< Type `char`, character type  */   \
    KeywordDef("string",   6, TOK_STRING)         /**< Type `string`, string type  */   \
    KeywordDef("short",    5, TOK_SHORT)          /**< Type `short`, short integer  */   \
    KeywordDef("int",      3, TOK_INT)            /**< Type `int`, integer  */   \
    KeywordDef("long",     4, TOK_LONG)           /**< Type `long`, long integer  */   \
    KeywordDef("float",    5, TOK_FLOAT)          /**< Type `float`, single-precision floating point  */   \
    KeywordDef("double",   6, TOK_DOUBLE)         /**< Type `double`, double-precision floating point  */   \
    KeywordDef("bool",     4, TOK_BOOL)           /**< Type `bool`, boolean type  */   \
    /* Arithmetic operators */ \
    KeywordDef("+",        1, TOK_PLUS)               /**< Operator `+` addition  */   \
    KeywordDef("-",        1, TOK_MINUS)              /**< Operator `-` subtraction or unary minus  */   \
    KeywordDef("*",        1, TOK_STAR)               /**< Operator `*` multiplication or pointer dereference  */   \
    KeywordDef("/",        1, TOK_SLASH)              /**< Operator `/` division  */   \
    KeywordDef("%",        1, TOK_PERCENT)            /**< Operator `%` modulo (remainder)  */   \
    KeywordDef("++",       2, TOK_INCREMENT)          /**< Operator `++` increment  */   \
    KeywordDef("--",       2, TOK_DECREMENT)          /**< Operator `--` decrement  */   \
    /* Relational operators */ \
    KeywordDef("==",       2, TOK_EQ)                 /**< Operator `==` equal to comparison  */   \
    KeywordDef("!=",       2, TOK_NE)                 /**< Operator `!=` not equal to comparison  */   \
    KeywordDef("<",        2, TOK_LT)                 /**< Operator `<` less than comparison  */   \
    KeywordDef(">",        2, TOK_GT)                 /**< Operator `>` greater than comparison  */   \
    KeywordDef("<=",       2, TOK_LE)                 /**< Operator `<=` less than or equal to comparison  */   \
    KeywordDef(">=",       2, TOK_GE)                 /**< Operator `>=` greater than or equal to comparison  */   \
    /* Logical operators */ \
    KeywordDef("&&",       2, TOK_LOGICAL_AND)        /**< Operator `&&` logical AND  */   \
    KeywordDef("||",       2, TOK_LOGICAL_OR)         /**< Operator `||` logical OR  */   \
    KeywordDef("!",        1, TOK_LOGICAL_NOT)        /**< Operator `!` logical NOT  */   \
    /* Bitwise operators */ \
    KeywordDef("&",        1, TOK_BITWISE_AND)        /**< Operator `&` bitwise AND or address-of operator  */   \
    KeywordDef("|",        1, TOK_BITWISE_OR)         /**< Operator `|` bitwise OR  */   \
    KeywordDef("^",        1, TOK_BITWISE_XOR)        /**< Operator `^` bitwise XOR  */   \
    KeywordDef("~",        1, TOK_BITWISE_NOT)        /**< Operator `~` bitwise NOT  */   \
    KeywordDef("<<",       2, TOK_LEFT_SHIFT)         /**< Operator `<<` left shift  */   \
    KeywordDef(">>",       2, TOK_RIGHT_SHIFT)        /**< Operator `>>` right shift  */   \
    /* Assignment operator */ \
    KeywordDef("=",        1, TOK_ASSIGN)             /**< Operator `=` simple assignment  */   \
    KeywordDef("+=",       2, TOK_PLUS_ASSIGN)        /**< Operator `+=` add and assign  */   \
    KeywordDef("-=",       2, TOK_MINUS_ASSIGN)       /**< Operator `-=` subtract and assign  */   \
    KeywordDef("*=",       2, TOK_STAR_ASSIGN)        /**< Operator `*=` multiply and assign  */   \
    KeywordDef("/=",       2, TOK_SLASH_ASSIGN)       /**< Operator `/=` divide and assign  */   \
    KeywordDef("%=",       2, TOK_PERCENT_ASSIGN)     /**< Operator `%=` modulo and assign  */   \
    KeywordDef("&=",       2, TOK_AND_ASSIGN)         /**< Operator `&=` bitwise AND and assign  */   \
    KeywordDef("|=",       2, TOK_OR_ASSIGN)          /**< Operator `|=` bitwise OR and assign  */   \
    KeywordDef("^=",       2, TOK_XOR_ASSIGN)         /**< Operator `^=` bitwise XOR and assign  */   \
    KeywordDef("<<=",      3, TOK_LEFT_SHIFT_ASSIGN)  /**< Operator `<<=` left shift and assign  */   \
    KeywordDef(">>=",      3, TOK_RIGHT_SHIFT_ASSIGN) /**< Operator `>>=` right shift and assign  */   \
    /* Member access */ \
    KeywordDef("->",       2, TOK_ARROW)              /**< `->` structure/union pointer member access operator  */   \
    KeywordDef(".",        1, TOK_DOT)                /**< `.` structure/union member access operator  */   \
    KeywordDef(".(",       2, TOK_DOT_PAREN)          /**< `.(` NihaoC pointer dereference operator `.()`  */   \
    /* Other operators */ \
    KeywordDef("?=",       2, TOK_SAFE_ASSIGN)        /**< `?=` NihaoC safe assignment operator (with pointer checking)  */   \
    KeywordDef("?",        1, TOK_QUESTION)           /**< `?` question mark part of conditional (ternary) operator  */   \
    KeywordDef(":",        1, TOK_COLON)              /**< `:` colon part of conditional (ternary) operator or label suffix  */   \
    KeywordDef(",",        1, TOK_COMMA)              /**< `,` comma operator  */   \
    KeywordDef("..",       2, TOK_RANGE)              /**< `..` NihaoC array/slice range operator `[start..end]`  */   \
    /* Delimiters/Punctuators) */ \
    KeywordDef("#",        1, TOK_POUND)              /**< `#` statement terminator  */   \
    KeywordDef(";",        1, TOK_SEMICOLON)          /**< `;` statement terminator  */   \
    KeywordDef("(",        1, TOK_LPAREN)             /**< `(` left parenthesis, used for function calls and expression grouping  */   \
    KeywordDef(")",        1, TOK_RPAREN)             /**< `)` right parenthesis  */   \
    KeywordDef("[",        1, TOK_LBRACKET)           /**< `[` left square bracket, used for array subscript  */   \
    KeywordDef("]",        1, TOK_RBRACKET)           /**< `]` right square bracket  */   \
    KeywordDef("{",        1, TOK_LBRACE)             /**< `{` left brace, used for code blocks and initializer lists  */   \
    KeywordDef("}",        1, TOK_RBRACE)             /**< `}` right brace  */   \


#define KeywordDef(str,len,token) token,
typedef enum {
    /* --- End of file and invalid tokens --- */
    TOK_EOF = 0,        /**< End Of File, indicates that the source code has been fully read  */
    TOK_UNKNOWN,        /**< Unrecognized character or sequence, usually a lexical error  */

    /* --- Identifiers --- */
    TOK_IDENTIFIER,     /**< Identifier, e.g., variable names, function names, type names  */
    TOK__UNDEF,
    TOK__FLOW,
    TOK__STATIC,

    TOK_NEWLINE,
    TOK_ERROR,

    /* --- Constants (literals) --- */
    TOK_INT_CONST,      /**< Integer constant, e.g., `123`, `0x1A`  */
    TOK_FLOAT_CONST,    /**< Floating-point constant, e.g., `3.14`, `2.5e-3`  */
    TOK_CHAR_CONST,     /**< Character constant, e.g., `'a'`, `'\n'`  */
    TOK_STRING_LITERAL, /**< String literal, e.g., `"Hello"`  */

    TOK_DOT_CAST,       /**< NihaoC specified type dereference operator `.(type)`  */
    TOK_TERNARY,        /**< Ternary conditional operator `?:` overall representation (used by some parsers)  */
    TOK_ELLIPSIS,
    TOK_DOUBLE_COLON,

    KeywordDefTable

    TOK_COUNT
} TokenType;
#undef KeywordDef

/* Token string representation */
typedef struct {
    const char *name;
    const char *str;
    // int line;
    // int column;
} TokenInfo;

typedef struct {
    const char *str;
    int len;
    TokenType tok;
} KeywordEntry;

#endif /* TOKEN_H */
