#include "ncc.h"

#define KeywordDef(str,len,token) {str, #token},
TokenInfo token_table[]= {
    /* Fixed entries matching the enum prefix (must stay in sync with token.h) */
    {"TOK_EOF",           "EOF"},
    {"TOK_UNKNOWN",       "unknown"},
    {"TOK_IDENTIFIER",    "identifier"},
    {"TOK_NEWLINE",       "newline"},
    {"TOK_ERROR",         "error"},
    {"TOK_INT_CONST",     "integer"},
    {"TOK_FLOAT_CONST",   "float"},
    {"TOK_CHAR_CONST",    "char"},
    {"TOK_STRING_LITERAL","string"},
    {"TOK_DOT_CAST",      ".()"},
    {"TOK_TERNARY",       "?:"},
    {"TOK_ELLIPSIS",      "..."},
    {"TOK_DOUBLE_COLON",  "::"},
    {"TOK_SAFE_DOT",      "?."},
    _KeywordDefTable_
};
#undef KeywordDef



#define KeywordDef(str,len,tok) {str, len, tok},
KeywordEntry keywords[] = {
    _KeywordDefTable_
    {NULL, 0, 0}      /* sentinel: is_keyword() scans until str==NULL */
};
#undef KeywordDef



/* ============================================================
 * Utility Functions
 * ============================================================ */

const char *token_name(TokenType tok)
{
    if (tok >= 0 && tok < TOK_COUNT) {
        return token_table[tok].name;
    }
    return "UNKNOWN";
}

int is_keyword(const char *str, int len)
{
    for (int i = 0; keywords[i].str; i++) {
        if (keywords[i].len == len &&
                memcmp(keywords[i].str, str, len) == 0) {
            return keywords[i].tok;
        }
    }
    return 0;
}

int is_type_token(TokenType tok)
{
    switch (tok) {
        case TOK_VOID: case TOK_CHAR: case TOK_STRING: case TOK_BOOL:
        case TOK_U8: case TOK_U16: case TOK_U32: case TOK_U64:
        case TOK_I8: case TOK_I16: case TOK_I32: case TOK_I64:
        case TOK_F32: case TOK_F64:
        case TOK_FX32: case TOK_FX64:
        case TOK_STRUCT: case TOK_UNION: case TOK_ENUM:
            return 1;
        default:
            return 0;
    }
}

int is_visibility_token(TokenType tok)
{
    return tok == TOK__FLOW || tok == TOK__STATIC || tok == TOK__UNDEF;
}

/* ============================================================
 * Single Character Helpers
 * ============================================================ */

static inline int lex_char(LexerState *lex)
{
    if (lex->buf_ptr < lex->buf_end) {
        unsigned char c = *lex->buf_ptr++;
        if (c == '\n') {
            lex->line_num++;
            lex->col_num = 1;
        } else {
            lex->col_num++;
        }
        return c;
    }
    return EOF;
}

static inline void lex_unget(LexerState *lex)
{
    if (lex->buf_ptr > lex->buffer) {
        lex->buf_ptr--;
        lex->col_num--;
    }
}

static inline int lex_peek_char(LexerState *lex)
{
    if (lex->buf_ptr < lex->buf_end) {
        return (unsigned char)*lex->buf_ptr;
    }
    return EOF;
}

/* ============================================================
 * Skip Whitespace and Comments
 * ============================================================ */

static void skip_whitespace(LexerState *lex)
{
    /* Clear any stale token from a previous lexer_next() call so that
     * a leftover TOK_NEWLINE is not mistaken for a new one. */
    lex->tok = TOK_UNKNOWN;

    for (;;) {
        int ch = lex_peek_char(lex);
        
        if (ch == EOF) break;
        
        /* Whitespace */
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            lex_char(lex);
            continue;
        }
        
        /* Newline - significant in NihaoC! */
        if (ch == '\n') {
            lex_char(lex);
            /* Only emit newline tokens at statement level */
            if (lex->paren_depth == 0 && lex->brace_depth == 0
                && lex->bracket_depth == 0) {
                lex->tok = TOK_NEWLINE;
                return;
            }
            continue;
        }
        
        /* Line comment */
        if (ch == '/' && lex->buf_ptr + 1 < lex->buf_end
            && *(lex->buf_ptr + 1) == '/') {
            lex_char(lex); lex_char(lex); /* skip // */
            while (lex_peek_char(lex) != '\n' && lex_peek_char(lex) != EOF) {
                lex_char(lex);
            }
            continue;
        }
        
        /* Block comment */
        if (ch == '/' && lex->buf_ptr + 1 < lex->buf_end
            && *(lex->buf_ptr + 1) == '*') {
            lex_char(lex); lex_char(lex); /* skip /* */
            for (;;) {
                if (lex_peek_char(lex) == EOF) {
                    lex->tok = TOK_ERROR;
                    return;
                }
                if (lex_peek_char(lex) == '*' && lex->buf_ptr + 1 < lex->buf_end
                    && *(lex->buf_ptr + 1) == '/') {
                    lex_char(lex); lex_char(lex); /* skip */
                    break;
                }
                lex_char(lex);
            }
            continue;
        }
        
        break;
    }
}

/* ============================================================
 * Number Parsing
 * ============================================================ */

static void parse_number(LexerState *lex, int first_char)
{
    char buf[128];
    int i = 0;
    int is_float = 0;
    int is_hex = 0;
    int is_binary = 0;
    int ch;
    
    buf[i++] = first_char;
    
    /* Check for hex prefix (0x or 0X) */
    if (first_char == '0') {
        ch = lex_peek_char(lex);
        if (ch == 'x' || ch == 'X') {
            is_hex = 1;
            buf[i++] = lex_char(lex);
        } else if (ch == 'b' || ch == 'B') {
            is_binary = 1;
            buf[i++] = lex_char(lex);
        }
    }
    
    /* Read digits */
    for (;;) {
        ch = lex_peek_char(lex);
        
        if (is_hex) {
            if (!isxdigit(ch)) break;
        } else if (is_binary) {
            if (ch != '0' && ch != '1') break;
        } else {
            if (!isdigit(ch)) break;
        }
        buf[i++] = lex_char(lex);
    }
    
    /* Floating point? */
    ch = lex_peek_char(lex);
    if (ch == '.') {
        char next = (lex->buf_ptr + 1 < lex->buf_end) ? *(lex->buf_ptr + 1) : 0;
        if (isdigit(next)) {
            is_float = 1;
            buf[i++] = lex_char(lex);
            while (isdigit(lex_peek_char(lex))) {
                buf[i++] = lex_char(lex);
            }
        }
    }
    
    /* Exponent? */
    ch = lex_peek_char(lex);
    if (ch == 'e' || ch == 'E') {
        is_float = 1;
        buf[i++] = lex_char(lex);
        ch = lex_peek_char(lex);
        if (ch == '+' || ch == '-') {
            buf[i++] = lex_char(lex);
        }
        while (isdigit(lex_peek_char(lex))) {
            buf[i++] = lex_char(lex);
        }
    }
    
    /* Type suffix */
    ch = lex_peek_char(lex);
    if (ch == 'f' || ch == 'F') {
        is_float = 1;
        lex_char(lex);
    }
    
    buf[i] = '\0';
    
    if (is_float) {
        lex->tok = TOK_FLOAT_CONST;
        lex->tok_val.f = strtod(buf, NULL);
    } else {
        lex->tok = TOK_INT_CONST;
        lex->tok_val.i = strtoll(buf, NULL, 0);
    }
}

/* ============================================================
 * String Literal Parsing
 * ============================================================ */

static void parse_string(LexerState *lex, int quote_char)
{
    char *buf;
    int buf_size = 256;
    int i = 0;
    int ch;
    
    buf = malloc(buf_size);
    
    for (;;) {
        ch = lex_char(lex);
        
        if (ch == EOF || ch == '\n') {
            /* Unterminated string */
            lex->tok = TOK_ERROR;
            free(buf);
            return;
        }
        
        if (ch == quote_char) {
            break;
        }
        
        /* Escape sequences */
        if (ch == '\\') {
            ch = lex_char(lex);
            switch (ch) {
                case 'n':  ch = '\n'; break;
                case 't':  ch = '\t'; break;
                case 'r':  ch = '\r'; break;
                case '\\': ch = '\\'; break;
                case '\'': ch = '\''; break;
                case '\"': ch = '\"'; break;
                case '0':  ch = '\0'; break;
                case 'x':
                    /* Hex escape */
                    ch = 0;
                    for (int j = 0; j < 2; j++) {
                        int h = lex_char(lex);
                        if (h >= '0' && h <= '9') ch = (ch << 4) | (h - '0');
                        else if (h >= 'a' && h <= 'f') ch = (ch << 4) | (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') ch = (ch << 4) | (h - 'A' + 10);
                        else break;
                    }
                    break;
                default:
                    /* Unknown escape - keep as-is */
                    break;
            }
        }
        
        /* Expand buffer if needed */
        if (i + 2 >= buf_size) {
            buf_size *= 2;
            buf = realloc(buf, buf_size);
        }
        buf[i++] = ch;
    }
    
    buf[i] = '\0';
    
    if (quote_char == '\'') {
        lex->tok = TOK_CHAR_CONST;
        lex->tok_val.i = (i > 0) ? (unsigned char)buf[0] : 0;
    } else {
        lex->tok = TOK_STRING_LITERAL;
        /* Store string in compiler string table */
        /* TODO: Actually store in string table via cs */
    }
    
    lex->tok_str = buf;
    lex->tok_len = i;
}

/* ============================================================
 * Identifier and Keyword Parsing
 * ============================================================ */

static void parse_ident(LexerState *lex, int first_char)
{
    int i = 0;
    int ch;
    
    lex->ident_buf[i++] = first_char;
    
    for (;;) {
        ch = lex_peek_char(lex);
        if (isalnum(ch) || ch == '_') {
            if (i < (int)sizeof(lex->ident_buf) - 1) {
                lex->ident_buf[i++] = lex_char(lex);
            } else {
                lex_char(lex);
            }
        } else {
            break;
        }
    }
    
    lex->ident_buf[i] = '\0';
    
    /* Check if keyword */
    int kw = is_keyword(lex->ident_buf, i);
    if (kw) {
        lex->tok = kw;
    } else {
        lex->tok = TOK_IDENTIFIER;
    }
    
    /* Independent copy: parser may keep tok_str across next_tok()/peek().
     * Never freed (short-lived compiler process); keeps ptr valid. */
    lex->tok_str = strdup(lex->ident_buf);
    lex->tok_len = i;
}

/* ============================================================
 * Main Lexer Function
 * ============================================================ */

void lexer_init(CompilerState *cs, const char *filename, const char *source)
{
    LexerState *lex = cs->parser.lex;
    
    lex->filename = (char *)filename;
    lex->buffer = (char *)source;
    lex->buf_ptr = (char *)source;
    lex->buf_end = (char *)source + strlen(source);
    lex->line_num = 1;
    lex->col_num = 1;
    lex->last_line_num = 1;
    
    lex->tok = TOK_UNKNOWN;
    lex->tok_val.i = 0;
    lex->tok_str = NULL;
    lex->tok_len = 0;
    
    lex->peek_tok = TOK_UNKNOWN;
    lex->peek_valid = 0;
    lex->peek_str = NULL;
    
    lex->paren_depth = 0;
    lex->brace_depth = 0;
    lex->bracket_depth = 0;
}

void lexer_next(LexerState *lex)
{
    int ch;
    
    /* Skip whitespace and comments */
    skip_whitespace(lex);
    
    /* If skip_whitespace set a token (newline), return it */
    if (lex->tok == TOK_NEWLINE) {
        return;
    }
    
    lex->last_line_num = lex->line_num;
    
    ch = lex_char(lex);
    
    if (ch == EOF) {
        lex->tok = TOK_EOF;
        return;
    }
    
    /* Track delimiters for newline significance */
    switch (ch) {
        case '(': lex->paren_depth++; break;
        case ')': lex->paren_depth--; break;
        case '{': lex->brace_depth++; break;
        case '}': lex->brace_depth--; break;
        case '[': lex->bracket_depth++; break;
        case ']': lex->bracket_depth--; break;
    }
    
    /* Numbers */
    if (isdigit(ch)) {
        parse_number(lex, ch);
        return;
    }
    
    /* Identifiers and keywords */
    if (isalpha(ch) || ch == '_') {
        parse_ident(lex, ch);
        return;
    }
    
    /* String and character literals */
    if (ch == '\"' || ch == '\'') {
        parse_string(lex, ch);
        return;
    }
    
    /* Operators and delimiters */
    switch (ch) {
        case '+':
            if (lex_peek_char(lex) == '+') {
                lex_char(lex);
                lex->tok = TOK_INCREMENT;
            } else if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_PLUS_ASSIGN;
            } else {
                lex->tok = TOK_PLUS;
            }
            break;
            
        case '-':
            if (lex_peek_char(lex) == '-') {
                lex_char(lex);
                lex->tok = TOK_DECREMENT;
            } else if (lex_peek_char(lex) == '>') {
                lex_char(lex);
                lex->tok = TOK_ARROW;
            } else if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_MINUS_ASSIGN;
            } else {
                lex->tok = TOK_MINUS;
            }
            break;
            
        case '*':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_STAR_ASSIGN;
            } else {
                lex->tok = TOK_STAR;
            }
            break;
            
        case '/':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_SLASH_ASSIGN;
            } else {
                lex->tok = TOK_SLASH;
            }
            break;
            
        case '%':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_PERCENT_ASSIGN;
            } else {
                lex->tok = TOK_PERCENT;
            }
            break;
            
        case '&':
            if (lex_peek_char(lex) == '&') {
                lex_char(lex);
                lex->tok = TOK_LOGICAL_AND;
            } else if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_AND_ASSIGN;
            } else {
                lex->tok = TOK_BITWISE_AND;
            }
            break;
            
        case '|':
            if (lex_peek_char(lex) == '|') {
                lex_char(lex);
                lex->tok = TOK_LOGICAL_OR;
            } else if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_OR_ASSIGN;
            } else {
                lex->tok = TOK_BITWISE_OR;
            }
            break;
            
        case '^':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_XOR_ASSIGN;
            } else {
                lex->tok = TOK_BITWISE_XOR;
            }
            break;
            
        case '~':
            lex->tok = TOK_BITWISE_NOT;
            break;
            
        case '!':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_NE;
            } else {
                lex->tok = TOK_LOGICAL_NOT;
            }
            break;
            
        case '=':
            if (lex_peek_char(lex) == '>') {
                lex_char(lex);
                lex->tok = TOK_FAT_ARROW;
            } else if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_EQ;
            } else {
                lex->tok = TOK_ASSIGN;
            }
            break;
            
        case '<':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_LE;
            } else if (lex_peek_char(lex) == '<') {
                lex_char(lex);
                if (lex_peek_char(lex) == '=') {
                    lex_char(lex);
                    lex->tok = TOK_LEFT_SHIFT_ASSIGN;
                } else {
                    lex->tok = TOK_LEFT_SHIFT;
                }
            } else {
                lex->tok = TOK_LT;
            }
            break;
            
        case '>':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_GE;
            } else if (lex_peek_char(lex) == '>') {
                lex_char(lex);
                if (lex_peek_char(lex) == '=') {
                    lex_char(lex);
                    lex->tok = TOK_RIGHT_SHIFT_ASSIGN;
                } else {
                    lex->tok = TOK_RIGHT_SHIFT;
                }
            } else {
                lex->tok = TOK_GT;
            }
            break;
            
        case '?':
            if (lex_peek_char(lex) == '=') {
                lex_char(lex);
                lex->tok = TOK_SAFE_ASSIGN;
            } else if (lex_peek_char(lex) == '.') {
                /* ?.  safe dereference (longest match) */
                lex_char(lex);
                lex->tok = TOK_SAFE_DOT;
            } else {
                lex->tok = TOK_QUESTION;
            }
            break;
            
        case ':': lex->tok = TOK_COLON; break;
        case '.':
            if (lex_peek_char(lex) == '.') {
                lex_char(lex);
                if (lex_peek_char(lex) == '.') {
                    lex_char(lex);
                    lex->tok = TOK_ELLIPSIS;
                } else {
                    lex->tok = TOK_RANGE;
                }
            } else if (lex_peek_char(lex) == '(') {
                /* .(  typed dereference (longest match) */
                lex_char(lex);
                lex->tok = TOK_DOT_PAREN;
            } else {
                lex->tok = TOK_DOT;
            }
            break;
            
        case '(': lex->tok = TOK_LPAREN; break;
        case ')': lex->tok = TOK_RPAREN; break;
        case '{': lex->tok = TOK_LBRACE; break;
        case '}': lex->tok = TOK_RBRACE; break;
        case '[': lex->tok = TOK_LBRACKET; break;
        case ']': lex->tok = TOK_RBRACKET; break;
        case ';': lex->tok = TOK_SEMICOLON; break;
        case ',': lex->tok = TOK_COMMA; break;
        case '#': lex->tok = TOK_POUND; break;
        
        default:
            /* Unknown character */
            lex->tok = TOK_ERROR;
            fprintf(stderr, "Lexer error at line %d: unexpected character '%c' (0x%02x)\n",
                    lex->line_num, ch, ch);
            break;
    }
}

/* Peek at next token without consuming it */
void lexer_peek(LexerState *lex)
{
    if (!lex->peek_valid) {
        /* Save current state */
        int saved_line = lex->line_num;
        int saved_col = lex->col_num;
        char *saved_ptr = lex->buf_ptr;
        char *saved_str = lex->tok_str;
        int saved_len = lex->tok_len;
        TokenType saved_tok = lex->tok;
        int saved_paren = lex->paren_depth;
        int saved_brace = lex->brace_depth;
        int saved_bracket = lex->bracket_depth;
        
        lexer_next(lex);
        
        lex->peek_tok = lex->tok;
        lex->peek_str = lex->tok_str;   /* strdup 永久拷贝，restore 后仍有效 */
        lex->peek_valid = 1;
        
        /* Restore */
        lex->line_num = saved_line;
        lex->col_num = saved_col;
        lex->buf_ptr = saved_ptr;
        lex->tok_str = saved_str;
        lex->tok_len = saved_len;
        lex->tok = saved_tok;
        lex->paren_depth = saved_paren;
        lex->brace_depth = saved_brace;
        lex->bracket_depth = saved_bracket;
    }
}

