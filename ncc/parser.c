#include "ncc.h"


/* ============================================================
 * Parser Initialization
 * ============================================================ */

void parser_init(CompilerState *cs)
{
    cs->parser.cur_module = NULL;
    cs->parser.cur_func = NULL;
    cs->parser.cur_struct = NULL;
    cs->parser.scope_depth = 0;
    cs->parser.parse_flags = 0;
    cs->parser.const_wanted = 0;
    cs->parser.nocode_wanted = 0;
    cs->parser.macro_ptr = NULL;
    cs->parser.unget_buffer_enabled = 0;
    cs->parser.error_count = 0;
    cs->parser.warning_count = 0;

    /* Register built-in symbols */
    sym_register_builtins(cs);
}

/* ============================================================
 * Token Helpers
 * ============================================================ */

TokenType cur_tok(CompilerState *cs)
{
    return cs->parser.lex->tok;
}

void next_tok(CompilerState *cs)
{
    lexer_next(cs->parser.lex);
}

void expect(CompilerState *cs, TokenType tok)
{
    if (cur_tok(cs) != tok) {
        nihao_error(cs, "expected '%s', got '%s'",
                   token_name(tok), token_name(cur_tok(cs)));
    }
    next_tok(cs);
}

void skip_newlines(CompilerState *cs)
{
    while (cur_tok(cs) == TOK_NEWLINE) {
        next_tok(cs);
    }
}

static int is_type_begin(TokenType tok)
{
    return is_type_token(tok)
        || tok == TOK_KW_CONST || tok == TOK_KW_FLOW || tok == TOK_KW_STATIC
        || tok == TOK_KW_ALIAS || tok == TOK_KW_MULTIRETURN;
}

/* ============================================================
 * Type Parsing
 * ============================================================ */

void parse_type(CompilerState *cs, CType *type)
{
    TokenType tok = cur_tok(cs);

    memset(type, 0, sizeof(CType));

    switch (tok) {
        case TOK_KW_VOID:    type->kind = TYPE_VOID;    type->size = 0; break;
        case TOK_KW_CHAR:    type->kind = TYPE_CHAR;    type->size = 1; break;
        case TOK_KW_STRING:  type->kind = TYPE_STRING;  type->size = 8; break;
        case TOK_KW_BOOL:    type->kind = TYPE_BOOL;    type->size = 1; break;
        case TOK_KW_U8:      type->kind = TYPE_U8;      type->size = 1; break;
        case TOK_KW_U16:     type->kind = TYPE_U16;     type->size = 2; break;
        case TOK_KW_U32:     type->kind = TYPE_U32;     type->size = 4; break;
        case TOK_KW_U64:     type->kind = TYPE_U64;     type->size = 8; break;
        case TOK_KW_I8:      type->kind = TYPE_I8;      type->size = 1; break;
        case TOK_KW_I16:     type->kind = TYPE_I16;     type->size = 2; break;
        case TOK_KW_I32:     type->kind = TYPE_I32;     type->size = 4; break;
        case TOK_KW_I64:     type->kind = TYPE_I64;     type->size = 8; break;
        case TOK_KW_F32:     type->kind = TYPE_F32;     type->size = 4; break;
        case TOK_KW_F64:     type->kind = TYPE_F64;     type->size = 8; break;
        case TOK_KW_FX32:    type->kind = TYPE_FX32;    type->size = 4; break;
        case TOK_KW_FX64:    type->kind = TYPE_FX64;    type->size = 8; break;
        case TOK_KW_STRUCT:
            next_tok(cs);
            type->kind = TYPE_STRUCT;
            if (cur_tok(cs) == TOK_IDENT) {
                /* Named struct */
                Symbol *sym = sym_find(cs, cs->parser.lex->tok_str);
                if (!sym) {
                    sym = sym_push(cs, SYM_STRUCT,
                                   cs->parser.lex->tok_str, type);
                }
                type->sym = sym;
                next_tok(cs);
            }
            if (cur_tok(cs) == TOK_LBRACE) {
                /* Struct body */
                next_tok(cs); /* skip { */
                skip_newlines(cs);
                while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                    /* Parse member */
                    CType member_type;
                    parse_type(cs, &member_type);
                    expect(cs, TOK_IDENT);
                    char *member_name = cs->parser.lex->tok_str;
                    next_tok(cs);
                    /* Bitfield syntax: u8:1 */
                    if (cur_tok(cs) == TOK_COLON) {
                        next_tok(cs);
                        member_type.bit_size = cs->parser.lex->tok_val.i;
                        next_tok(cs);
                    }
                    skip_newlines(cs);
                    if (type->sym) {
                        sym_add_member(cs, type->sym, member_name, &member_type);
                    }
                }
                expect(cs, TOK_RBRACE);
            }
            return;
        case TOK_KW_UNION:
            /* Similar to struct parsing */
            type->kind = TYPE_UNION;
            next_tok(cs);
            /* TODO: Full union parsing */
            return;
        case TOK_KW_ENUM:
            type->kind = TYPE_ENUM;
            next_tok(cs);
            /* TODO: Full enum parsing */
            return;
        default:
            nihao_error(cs, "expected type, got '%s'", token_name(tok));
            return;
    }
    next_tok(cs);

    /* Check for pointer/array modifiers */
    while (cur_tok(cs) == TOK_STAR || cur_tok(cs) == TOK_LBRACKET) {
        if (cur_tok(cs) == TOK_STAR) {
            next_tok(cs);
            CType *ptr_type = type_new(cs, TYPE_POINTER);
            ptr_type->ref = type_new(cs, type->kind);
            memcpy(ptr_type->ref, type, sizeof(CType));
            *type = *ptr_type;
        } else if (cur_tok(cs) == TOK_LBRACKET) {
            next_tok(cs);
            int array_size = -1; /* dynamic */
            if (cur_tok(cs) == TOK_INT_LIT) {
                array_size = cs->parser.lex->tok_val.i;
                next_tok(cs);
            } else if (cur_tok(cs) == TOK_ELLIPSIS) {
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_LIT) {
                    array_size = cs->parser.lex->tok_val.i;
                    next_tok(cs);
                }
            }
            expect(cs, TOK_RBRACKET);
            CType *arr_type = type_array(cs, NULL, array_size);
            memcpy(arr_type->ref, type, sizeof(CType));
            *type = *arr_type;
        }
    }
}

/* ============================================================
 * Module Parsing
 * ============================================================ */

void parse_module(CompilerState *cs)
{
    skip_newlines(cs);

    /* Module declaration */
    if (cur_tok(cs) == TOK_KW_MODULE) {
        next_tok(cs);
        expect(cs, TOK_IDENT);

        char *mod_name = cs->parser.lex->tok_str;
        Module *mod = module_add(cs, mod_name, cs->input_file);
        cs->parser.cur_module = mod;
        next_tok(cs);
        skip_newlines(cs);
    } else {
        /* Default module */
        Module *mod = module_add(cs, "main", cs->input_file);
        cs->parser.cur_module = mod;
    }

    /* Parse use statements */
    while (cur_tok(cs) == TOK_KW_USE) {
        next_tok(cs);
        expect(cs, TOK_IDENT);
        char *use_name = cs->parser.lex->tok_str;
        module_import(cs, use_name);
        next_tok(cs);
        skip_newlines(cs);
    }

    /* Parse link statements */
    while (cur_tok(cs) == TOK_KW_LINK) {
        next_tok(cs);
        /* link "libhttp.so" as http */
        if (cur_tok(cs) == TOK_STR_LIT) {
            char *lib_path = cs->parser.lex->tok_str;
            next_tok(cs);
            if (cur_tok(cs) == TOK_KW_AS) {
                next_tok(cs);
                expect(cs, TOK_IDENT);
                char *alias = cs->parser.lex->tok_str;
                link_add_library(cs, lib_path, alias, lib_path);
                next_tok(cs);
            } else {
                link_add_library(cs, lib_path, lib_path, lib_path);
            }
        }
        skip_newlines(cs);
    }

    /* Parse top-level declarations */
    while (cur_tok(cs) != TOK_EOF) {
        parse_declaration(cs);
        skip_newlines(cs);
    }
}

/* ============================================================
 * Declaration Parsing
 * ============================================================ */

void parse_declaration(CompilerState *cs)
{
    TokenType tok = cur_tok(cs);
    Visibility vis = VIS_DEFAULT;
    int is_const_func = 0;
    int is_multireturn = 0;

    /* Visibility modifiers */
    if (tok == TOK_KW_CONST) {
        vis = VIS_CONST;
        next_tok(cs);
        /* const at top-level means function */
        if (cur_tok(cs) != TOK_KW_FLOW && cur_tok(cs) != TOK_KW_STATIC) {
            is_const_func = 1;
        }
    } else if (tok == TOK_KW_FLOW) {
        vis = VIS_FLOW;
        next_tok(cs);
    } else if (tok == TOK_KW_STATIC) {
        vis = VIS_STATIC;
        next_tok(cs);
    }

    /* Multireturn */
    // if (cur_tok(cs) == TOK_KW_MULTIRETURN) {
    //     is_multireturn = 1;
    //     next_tok(cs);
    // }

    /* Alias */
    if (cur_tok(cs) == TOK_KW_ALIAS) {
        next_tok(cs);
        expect(cs, TOK_IDENT);
        char *alias_name = cs->parser.lex->tok_str;
        next_tok(cs);
        expect(cs, TOK_ASSIGN);
        next_tok(cs);
        CType aliased_type;
        parse_type(cs, &aliased_type);
        sym_push(cs, SYM_TYPEDEF, alias_name, &aliased_type);
        return;
    }

    /* Type or function */
    if (is_type_begin(cur_tok(cs)) || cur_tok(cs) == TOK_IDENT) {
        CType base_type;
        int is_func = 0;

        if (is_type_begin(cur_tok(cs))) {
            parse_type(cs, &base_type);
        } else {
            /* Must be a function without explicit return type */
            base_type.kind = TYPE_VOID;
            base_type.size = 0;
            is_func = 1;
        }

        /* Check for function */
        if (cur_tok(cs) == TOK_LPAREN || is_func) {
            /* Function declaration or definition */
            Symbol *func_sym;
            char *func_name;

            if (is_func) {
                /* void was the identifier, backtrack */
                func_name = "main"; /* default */
            } else {
                /* The type was parsed, now expect function name */
                /* This is simplified - full parsing would be more complex */
                expect(cs, TOK_IDENT);
                func_name = cs->parser.lex->tok_str;
                next_tok(cs);
            }

            /* Create function symbol */
            CType *func_type = type_new(cs, TYPE_FUNC);
            func_sym = sym_push(cs, SYM_FUNCTION, func_name, func_type);
            func_sym->vis = vis;

            /* Parse parameters */
            expect(cs, TOK_LPAREN);
            if (cur_tok(cs) != TOK_RPAREN) {
                /* Parse parameter list */
                while (cur_tok(cs) != TOK_RPAREN && cur_tok(cs) != TOK_EOF) {
                    Visibility param_vis = VIS_DEFAULT;
                    if (cur_tok(cs) == TOK_KW_FLOW) {
                        param_vis = VIS_FLOW;
                        next_tok(cs);
                    } else if (cur_tok(cs) == TOK_KW_STATIC) {
                        param_vis = VIS_STATIC;
                        next_tok(cs);
                    }

                    CType param_type;
                    parse_type(cs, &param_type);
                    expect(cs, TOK_IDENT);
                    char *param_name = cs->parser.lex->tok_str;
                    next_tok(cs);

                    Symbol *param = sym_push_local(cs, func_sym, param_name, &param_type);
                    param->vis = param_vis;
                    param->next = func_sym->params;
                    func_sym->params = param;

                    if (cur_tok(cs) == TOK_COMMA) {
                        next_tok(cs);
                    }
                }
            }
            expect(cs, TOK_RPAREN);

            /* Multireturn */
            // if (cur_tok(cs) == TOK_KW_MULTIRETURN) {
            //     func_sym->type->is_multireturn = 1;
            //     next_tok(cs);
            // }

            /* Function body */
            if (cur_tok(cs) == TOK_LBRACE) {
                func_sym->is_defined = 1;
                parse_function(cs, func_sym);
            }
        } else {
            /* Variable declaration */
            expect(cs, TOK_IDENT);
            char *var_name = cs->parser.lex->tok_str;
            next_tok(cs);

            Symbol *var_sym;
            if (cs->parser.cur_func) {
                var_sym = sym_push_local(cs, cs->parser.cur_func, var_name, &base_type);
            } else {
                var_sym = sym_push(cs, SYM_VARIABLE, var_name, &base_type);
            }
            var_sym->vis = vis;

            /* Optional initializer */
            if (cur_tok(cs) == TOK_ASSIGN) {
                next_tok(cs);
                parse_expression(cs);
            }
        }
    }
}

/* ============================================================
 * Function Parsing
 * ============================================================ */

void parse_function(CompilerState *cs, Symbol *func_sym)
{
    Symbol *prev_func = cs->parser.cur_func;
    cs->parser.cur_func = func_sym;

    expect(cs, TOK_LBRACE);
    skip_newlines(cs);

    /* Generate function prologue */
    gen_function_prologue(func_sym);

    /* Parse function body */
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        parse_statement(cs);
        skip_newlines(cs);
    }

    expect(cs, TOK_RBRACE);

    /* Generate function epilogue */
    gen_function_epilogue(func_sym);

    /* Pop local symbols */
    sym_pop_locals(cs, func_sym);

    cs->parser.cur_func = prev_func;
}

/* ============================================================
 * Statement Parsing
 * ============================================================ */

void parse_statement(CompilerState *cs)
{
    TokenType tok = cur_tok(cs);

    switch (tok) {
        case TOK_KW_IF:
            next_tok(cs);
            parse_expression(cs);
            gen_if();
            parse_statement(cs);
            if (cur_tok(cs) == TOK_KW_ELSE) {
                next_tok(cs);
                parse_statement(cs);
            } else if (cur_tok(cs) == TOK_KW_ELIF) {
                next_tok(cs);
                parse_expression(cs);
                gen_if();
                parse_statement(cs);
            }
            break;

        case TOK_KW_WHILE:
            next_tok(cs);
            parse_expression(cs);
            gen_while();
            parse_statement(cs);
            break;

        case TOK_KW_FOR:
            next_tok(cs);
            parse_expression(cs); /* init */
            expect(cs, TOK_SEMICOLON);
            parse_expression(cs); /* condition */
            expect(cs, TOK_SEMICOLON);
            parse_expression(cs); /* increment */
            gen_for();
            parse_statement(cs);
            break;

        case TOK_KW_RETURN:
            next_tok(cs);
            if (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE) {
                parse_expression(cs);
            }
            gen_return();
            break;

        case TOK_KW_BREAK:
            next_tok(cs);
            /* TODO: Generate break */
            break;

        case TOK_KW_CONTINUE:
            next_tok(cs);
            /* TODO: Generate continue */
            break;

        case TOK_LBRACE:
            /* Block statement */
            next_tok(cs);
            skip_newlines(cs);
            vis_scope_enter(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                parse_statement(cs);
                skip_newlines(cs);
            }
            vis_scope_exit(cs);
            expect(cs, TOK_RBRACE);
            break;

        case TOK_KW_COOKING:
            /* Compile-time execution block */
            next_tok(cs);
            expect(cs, TOK_LBRACE);
            /* TODO: Execute at compile time */
            int depth = 1;
            while (depth > 0) {
                if (cur_tok(cs) == TOK_LBRACE) depth++;
                else if (cur_tok(cs) == TOK_RBRACE) depth--;
                next_tok(cs);
            }
            break;

        default:
            /* Expression statement */
            parse_expression(cs);
            break;
    }
}

/* ============================================================
 * Expression Parsing (Placeholder)
 * ============================================================ */

void parse_expression(CompilerState *cs)
{
    /* This is a simplified expression parser.
     * A full implementation would use operator precedence parsing,
     * similar to TCC's expr() function in tccgen.c
     */

    /* For now, just skip to statement terminator */
    while (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_SEMICOLON
           && cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_RPAREN
           && cur_tok(cs) != TOK_COMMA && cur_tok(cs) != TOK_EOF) {
        next_tok(cs);
    }

    if (cur_tok(cs) == TOK_SEMICOLON) {
        next_tok(cs);
    }
}


/*
 * Updated parser sections to integrate all new features
 * Add these to the original parser.c at the appropriate locations
 */

/* ============================================================
 * Updated parse_function to use full code generation
 * ============================================================ */

void parse_function_full(CompilerState *cs, Symbol *func_sym) {
    Symbol *prev_func = cs->parser.cur_func;
    cs->parser.cur_func = func_sym;

    // /* Check for multireturn type */
    // infer_multireturn_type(cs, func_sym);

    expect(cs, TOK_LBRACE);
    skip_newlines(cs);

    /* Generate function prologue with register saving */
    gen_function_prologue_full(func_sym);

    /* Parse function body */
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        /* Type check statement before generating */
        type_check_statement(cs);

        /* Parse and generate code */
        parse_statement(cs);
        skip_newlines(cs);
    }

    expect(cs, TOK_RBRACE);

    /* Generate function epilogue */
    gen_function_epilogue_full(func_sym);

    /* Pop local symbols */
    sym_pop_locals(cs, func_sym);

    cs->parser.cur_func = prev_func;
}

/* ============================================================
 * Updated statement parsing to dispatch to new code generators
 * ============================================================ */

void parse_statement_full(CompilerState *cs) {
    TokenType tok = cur_tok(cs);

    switch (tok) {
        case TOK_KW_IF:
            next_tok(cs);
            parse_expression(cs);
            gen_if_statement(cs);  /* Full IF code generation */
            break;

        case TOK_KW_WHILE:
            next_tok(cs);
            parse_expression(cs);
            gen_while_loop(cs);  /* Full WHILE code generation */
            break;

        case TOK_KW_FOR:
            next_tok(cs);
            /* Init already executed */
            parse_expression(cs); /* Init */
            expect(cs, TOK_SEMICOLON);
            parse_expression(cs); /* Condition */
            expect(cs, TOK_SEMICOLON);
            parse_expression(cs); /* Increment */
            gen_for_loop(cs);  /* Full FOR code generation */
            break;

        case TOK_KW_DO:
            next_tok(cs);
            gen_do_while_loop(cs);
            break;

        case TOK_KW_RETURN:
            next_tok(cs);
            if (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE
                && cur_tok(cs) != TOK_SEMICOLON) {

                // if (cur_tok(cs) == TOK_KW_MULTIRETURN) {
                //     parse_multireturn(cs);
                // } else {
                //     parse_expression(cs);
                // }
                parse_expression(cs);
            }
            gen_return_statement(cs);
            break;

        case TOK_KW_BREAK:
            next_tok(cs);
            /* TODO: Emit jump to loop exit */
            break;

        case TOK_KW_CONTINUE:
            next_tok(cs);
            /* TODO: Emit jump to loop increment/condition */
            break;

        case TOK_KW_MATCH:
            gen_match_statement(cs);
            break;

        case TOK_KW_SELECT:
            gen_select_statement(cs);
            break;

        case TOK_LBRACE:
            /* Block */
            next_tok(cs);
            skip_newlines(cs);
            vis_scope_enter(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                parse_statement_full(cs);
                skip_newlines(cs);
            }
            vis_scope_exit(cs);
            expect(cs, TOK_RBRACE);
            break;

        case TOK_KW_COOKING:
            // parse_cooking_block(cs);
            // cooking_generate_code(cs);
            break;

        default:
            /* Expression statement or declaration */
            if (is_type_begin(cur_tok(cs)) || cur_tok(cs) == TOK_KW_ALIAS) {
                parse_declaration(cs);
            } else {
                parse_expression(cs);
            }
            break;
    }

    /* Consume optional semicolon */
    if (cur_tok(cs) == TOK_SEMICOLON) {
        next_tok(cs);
    }
}

/* ============================================================
 * Updated compilation entry to apply optimizations
 * ============================================================ */

static int compile_file_full(CompilerState *cs, const char *filename) {
    char *source;
    size_t source_size;

    /* Load file */
    source = load_source_file(filename, &source_size);
    if (!source) return -1;

    if (cs->verbose) {
        printf("Compiling: %s (%zu bytes)\n", filename, source_size);
    }

    /* Initialize subsystems */
    LexerState *lex = nihao_malloc(cs, sizeof(LexerState));
    cs->parser.lex = lex;
    lexer_init(cs, filename, source);

    parser_init(cs);
    codegen_init(cs);
    visibility_init(cs);
    linker_init(cs);

    /* Register standard library */
    stdlib_register_all(cs);
    stdlib_resolve_link_libraries(cs);
    stdlib_generate_runtime_stubs(cs);

    /* Parse module */
    parse_module(cs);

    /* Type verification pass */
    verify_types(cs);

    /* Apply peephole optimizations */
    codegen_optimize(cs);

    /* Check errors */
    if (cs->error_count > 0) {
        fprintf(stderr, "Compilation failed with %d error(s), %d warning(s)\n",
                cs->error_count, cs->warning_count);
        return -1;
    }

    /* Generate output */
    switch (cs->output_type) {
        case 0: /* Executable */
            linker_generate_executable_full(cs, cs->output_file);
            break;
        case 1: /* Object file */
            linker_generate_object(cs, cs->output_file);
            break;
        case 2: /* Shared library */
            break;
        case 3: /* Static library */
            break;
    }

    if (cs->verbose) {
        printf("Compilation successful (%d warnings)\n", cs->warning_count);
    }

    return 0;
}

