#include "ncc.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

CompilerState *g_cs;

/* Create parent directories for the given file path (no-op if exists). */
static void ensure_parent_dir(const char *path)
{
    char tmp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
}

/* ============================================================
 * Memory Management
 * ============================================================ */

void *nihao_malloc(CompilerState *cs, size_t size)
{
    void *ptr;
    ptr = malloc(size);
    if (!ptr) {
        nihao_error(cs, "memory allocation failed (size=%zu)", size);
        exit(1);
    }
    memset(ptr, 0, size);

    /* Track allocation for cleanup */
    if (cs->alloc_count >= cs->alloc_capacity) {
        cs->alloc_capacity = cs->alloc_capacity ? cs->alloc_capacity * 2 : 256;
        cs->allocated_ptrs = realloc(cs->allocated_ptrs,
                                     cs->alloc_capacity * sizeof(void *));
    }
    cs->allocated_ptrs[cs->alloc_count++] = ptr;

    return ptr;
}

void *nihao_realloc(CompilerState *cs, void *ptr, size_t size)
{
    void *new_ptr;
    /* Find and update tracked pointer */
    for (int i = 0; i < cs->alloc_count; i++) {
        if (cs->allocated_ptrs[i] == ptr) {
            new_ptr = realloc(ptr, size);
            if (!new_ptr) {
                nihao_error(cs, "memory reallocation failed (size=%zu)", size);
                exit(1);
            }
            cs->allocated_ptrs[i] = new_ptr;
            return new_ptr;
        }
    }
    new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        nihao_error(cs, "memory reallocation failed (size=%zu)", size);
        exit(1);
    }
    return new_ptr;
}

char *nihao_strdup(CompilerState *cs, const char *str)
{
    char *s;
    size_t len = strlen(str) + 1;
    s = nihao_malloc(cs, len);
    memcpy(s, str, len);
    return s;
}

/* ============================================================
 * Error & Warning Reporting
 * ============================================================ */

void nihao_error(CompilerState *cs, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fprintf(stderr, "\033[1;31mError\033[0m: %s\n", buf);
    if (cs->parser.lex) {
        fprintf(stderr, "  at %s:%d:%d\n",
                cs->parser.lex->filename,
                cs->parser.lex->line_num,
                cs->parser.lex->col_num);
    }
    cs->error_count++;
}

void nihao_warning(CompilerState *cs, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    if (!cs->verbose) return;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fprintf(stderr, "\033[1;33mWarning\033[0m: %s\n", buf);
    if (cs->parser.lex) {
        fprintf(stderr, "  at %s:%d:%d\n",
                cs->parser.lex->filename,
                cs->parser.lex->line_num,
                cs->parser.lex->col_num);
    }
    cs->warning_count++;
}

/* ============================================================
 * Initialization & Cleanup
 * ============================================================ */

static CompilerState *nihao_new(void)
{
    CompilerState *cs;

    cs = malloc(sizeof(CompilerState));
    if (!cs) {
        fprintf(stderr, "Fatal: failed to allocate compiler state\n");
        exit(1);
    }
    memset(cs, 0, sizeof(CompilerState));

    /* Default configuration */
    cs->output_type = 0;        /* executable */
    cs->verbose = 0;
    cs->debug_mode = 0;

    /* Initialize subsystems */
    cs->allocated_ptrs = NULL;
    cs->alloc_count = 0;
    cs->alloc_capacity = 0;

    /* Setup parser state pointer */
    cs->parser.cs = cs;
    cs->parser.lex = NULL;

    /* Setup code generator state pointer */
    cs->codegen.cs = cs;

    return cs;
}

static void nihao_cleanup(CompilerState *cs)
{
    if (!cs) return;

    /* Free all tracked allocations */
    for (int i = 0; i < cs->alloc_count; i++) {
        free(cs->allocated_ptrs[i]);
    }
    free(cs->allocated_ptrs);

    /* Close output file */
    if (cs->outfile && cs->outfile != stdout) {
        fclose(cs->outfile);
    }

    free(cs);
}

/* ============================================================
 * Command Line Parsing
 * ============================================================ */

static void print_usage(void)
{
    printf(
        "NihaoC Compiler v" NIHAO_VERSION "\n"
        "Usage: nihao <command> [options]\n"
        "\n"
        "Commands:\n"
        "  init [name]     Create a new project (nihao.toml + src/main.nc)\n"
        "  build <file>    Compile to an executable (keep <out>.c intermediate)\n"
        "  run <file>      Compile and run, extra args after '--' passed to program\n"
        "  debug <file>    Compile with verbose info, show generated C, then run\n"
        "  test            Run the regression test suite (tests/)\n"
        "  lex <file>      Dump the token stream of a source file\n"
        "\n"
        "Options:\n"
        "  -o <file>       Output file name\n"
        "  -c              Compile only (object file)\n"
        "  -shared         Generate shared library\n"
        "  -static         Generate static library\n"
        "  -backend <be>   Backend: c (default, external tcc) | native (libtcc)\n"
        "  -run            Native backend: compile to memory and run (Linux only)\n"
        "  -v, --verbose   Verbose output\n"
        "  -g              Generate debug information\n"
        "  -I <dir>        Add include directory\n"
        "  -L <dir>        Add library directory\n"
        "  -l <lib>        Link with library\n"
        "  --link <lib> as <alias>  Link library with alias\n"
        "  -h, --help      Show this help\n"
        "  --version       Show version\n"
        "\n"
        "Examples:\n"
        "  nihao init myapp\n"
        "  nihao build src/main.nc -o bin/app\n"
        "  nihao run src/main.nc -- hello world\n"
    );
}

static void lexer_test(CompilerState *cs, const char *filename);
static int parse_args(CompilerState *cs, int argc, char **argv)
{
    int i;
    int test_mode = 0;

    cs->argc = argc;
    cs->argv = argv;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (arg[0] == '-') {
            /* Output file */
            if (strcmp(arg, "-o") == 0) {
                if (i + 1 < argc) {
                    cs->output_file = argv[++i];
                } else {
                    fprintf(stderr, "Error: -o requires an argument\n");
                    return -1;
                }
            }
            /* Compile only */
            else if (strcmp(arg, "-c") == 0) {
                cs->output_type = 1;
            }
            /* Shared library */
            else if (strcmp(arg, "-shared") == 0) {
                cs->output_type = 2;
            }
            /* Static library */
            else if (strcmp(arg, "-static") == 0) {
                cs->output_type = 3;
            }
            /* Verbose */
            else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
                cs->verbose = 1;
            }
            /* Debug */
            else if (strcmp(arg, "-g") == 0) {
                cs->debug_mode = 1;
            }
            /* Run in memory (native backend only): compile and execute main */
            else if (strcmp(arg, "-run") == 0) {
                cs->run_mode = 1;
                cs->backend = 1;
            }
            /* Backend: c | native | ir-c (IR->C) | ir-native (IR->x86-64 asm) */
            else if (strncmp(arg, "-backend=", 9) == 0) {
                const char *be = arg + 9;
                if (strcmp(be, "native") == 0) {
                    cs->backend = 1;
                } else if (strcmp(be, "c") == 0) {
                    cs->backend = 0;
                } else if (strcmp(be, "ir-c") == 0) {
                    cs->backend = 2;
                } else if (strcmp(be, "ir-native") == 0) {
                    cs->backend = 3;
                } else {
                    fprintf(stderr, "Error: unknown backend '%s' "
                            "(c|native|ir-c|ir-native)\n", be);
                    return -1;
                }
            }
            else if (strcmp(arg, "-backend") == 0) {
                if (i + 1 < argc) {
                    const char *be = argv[++i];
                    if (strcmp(be, "native") == 0) {
                        cs->backend = 1;
                    } else if (strcmp(be, "c") == 0) {
                        cs->backend = 0;
                    } else if (strcmp(be, "ir-c") == 0) {
                        cs->backend = 2;
                    } else if (strcmp(be, "ir-native") == 0) {
                        cs->backend = 3;
                    } else {
                        fprintf(stderr, "Error: unknown backend '%s' "
                                "(c|native|ir-c|ir-native)\n", be);
                        return -1;
                    }
                } else {
                    fprintf(stderr, "Error: -backend requires an argument "
                                    "(c|native|ir-c|ir-native)\n");
                    return -1;
                }
            }
            /* Include directory */
            else if (strcmp(arg, "-I") == 0) {
                if (i + 1 < argc) {
                    /* TODO: Add include path */
                    i++;
                }
            }
            /* Library directory */
            else if (strcmp(arg, "-L") == 0) {
                if (i + 1 < argc) {
                    /* TODO: Add library path */
                    i++;
                }
            }
            /* Link library */
            else if (strcmp(arg, "-l") == 0) {
                if (i + 1 < argc) {
                    /* TODO: Add standard library */
                    i++;
                }
            }
            /* Link with alias */
            else if (strcmp(arg, "--link") == 0) {
                if (i + 3 < argc && strcmp(argv[i + 2], "as") == 0) {
                    // link_add_library(cs, argv[i+1], argv[i+3], argv[i+1]);
                    i += 3;
                } else {
                    fprintf(stderr, "Error: --link requires: <lib> as <alias>\n");
                    return -1;
                }
            }
            /* Help */
            else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
                print_usage();
                exit(0);
            }
            /* Version */
            else if (strcmp(arg, "--version") == 0) {
                printf("NihaoC Compiler v" NIHAO_VERSION "\n");
                exit(0);
            }
            /* Debug */
            else if (strcmp(arg, "-lexertest") == 0) {
                cs->input_file  = argv[2];
                test_mode = 1;
                break;
            }
            /* Unknown option */
            else {
                fprintf(stderr, "Warning: unknown option '%s'\n", arg);
            }
        }
        else {
            /* Input file */
            cs->input_file = (char *)arg;
        }
    }

    if (test_mode) {
        if (!cs->input_file) {
            fprintf(stderr, "Error: no input file specified\n");
            print_usage();
            return -1;
        }
        cs->test_mode = 1;
        puts("ncc testing!");
        lexer_test(cs, cs->input_file);
        return 0;
    }

    if (!cs->input_file) {
        fprintf(stderr, "Error: no input file specified\n");
        print_usage();
        return -1;
    }

    /* Set default output file if not specified */
    if (!cs->output_file) {
        if (cs->output_type == 1) {
            /* Object file: replace extension with .o */
            char *dot = strrchr(cs->input_file, '.');
            char *out = malloc(strlen(cs->input_file) + 3);
            if (dot) {
                int len = dot - cs->input_file;
                memcpy(out, cs->input_file, len);
                out[len] = '\0';
            } else {
                strcpy(out, cs->input_file);
            }
            strcat(out, ".o");
            cs->output_file = out;
        } else if (cs->output_type == 2) {
            cs->output_file = "a.so";
        } else {
            cs->output_file = "a.out";
        }
    }

    return 0;
}

/* ============================================================
 * Source File Loading
 * ============================================================ */

char *load_source_file(const char *filename, size_t *size_out)
{
    FILE *fp;
    char *buffer;
    size_t size;

    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        return NULL;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Allocate buffer with extra null terminator */
    buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: memory allocation failed\n");
        fclose(fp);
        return NULL;
    }

    /* Read file */
    if (fread(buffer, 1, size, fp) != size) {
        fprintf(stderr, "Error: failed to read file '%s'\n", filename);
        free(buffer);
        fclose(fp);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(fp);

    if (size_out) *size_out = size;
    return buffer;
}

/* ============================================================
 * Main Compilation Entry
 * ============================================================ */

static int compile_file(CompilerState *cs, const char *filename)
{
    char *source;
    size_t source_size;

    /* IR middle-layer pipeline: -backend=ir-c (2) / ir-native (3) */
    if (cs->backend >= 2) {
        return ir_compile(cs, filename, cs->backend, cs->verbose);
    }

    /* Load source file */
    source = load_source_file(filename, &source_size);
    if (!source) return -1;

    if (cs->verbose) {
        printf("Compiling: %s (%zu bytes)\n", filename, source_size);
    }

    /* Initialize lexer */
    LexerState *lex = nihao_malloc(cs, sizeof(LexerState));
    cs->parser.lex = lex;
    lexer_init(cs, filename, source);

    /* Initialize subsystems */
    parser_init(cs);
    codegen_init(cs);
    visibility_init(cs);
    linker_init(cs);
    stdlib_register_all(cs);
    stdlib_resolve_link_libraries(cs);
    stdlib_generate_runtime_stubs(cs);

    /* Initialize C backend */
    cgen_init();
    cgen_header();

    /* Parse module (emits C source via cgen) */
    parse_module(cs);

    /* Check for errors */
    if (cs->error_count > 0) {
        fprintf(stderr, "Compilation failed with %d error(s), %d warning(s)\n",
                cs->error_count, cs->warning_count);
        return -1;
    }

    if (cs->verbose) {
        printf("Compilation successful (%d warnings)\n", cs->warning_count);
    }

    /* Write the generated C source to <output>.c */
    char cpath[1024];
    snprintf(cpath, sizeof(cpath), "%s.c", cs->output_file ? cs->output_file : "a.out");
    ensure_parent_dir(cpath);
    FILE *cfp = fopen(cpath, "wb");
    if (!cfp) {
        nihao_error(cs, "cannot open C output file '%s'", cpath);
        return -1;
    }
    fputs(cgen_result(), cfp);
    fclose(cfp);
    if (cs->verbose) {
        printf("C source written to %s (%d bytes)\n", cpath,
               (int)strlen(cgen_result()));
    }

    /* Produce the final executable: -backend=c -> external tcc,
     * -backend=native -> libtcc in-process machine code,
     * -run (native) -> compile to memory and execute main directly */
    if (cs->output_type == 0) {
        const char *out = cs->output_file ? cs->output_file : "a.out";
        if (cs->backend == 1) {
            if (cs->run_mode) {
#ifdef _WIN32
                fprintf(stderr, "Error: -run (in-memory execution) is not supported "
                                "by the Windows libtcc build; use -backend=native "
                                "with an output file instead\n");
                return -1;
#else
                if (cs->verbose) {
                    printf("native backend: compiling %d bytes of C to memory\n",
                           (int)strlen(cgen_result()));
                }
                char *run_argv[] = { "nihao-run", NULL };
                return native_run_string(cgen_result(), 1, run_argv, cs->verbose);
#endif
            }
            if (cs->verbose) {
                printf("native backend: libtcc compiling %d bytes of C\n",
                       (int)strlen(cgen_result()));
            }
            if (native_compile_string(cgen_result(), out, cs->verbose) != 0) {
                fprintf(stderr, "native backend failed\n");
                return -1;
            }
        } else {
            char cmd[1600];
            snprintf(cmd, sizeof(cmd), "tcc \"%s\" -o \"%s\"", cpath, out);
            if (cs->verbose) {
                printf("Invoking: %s\n", cmd);
            }
            int rc = system(cmd);
            if (rc != 0) {
                fprintf(stderr, "tcc backend failed (exit %d)\n", rc);
                return -1;
            }
        }
    }

    return 0;
}

static void lexer_test(CompilerState *cs, const char *filename)
{
    size_t source_size = 0;
    char *source = load_source_file(filename, &source_size);
    if (!source) return;

    if (cs->verbose) {
        printf("Compiling: %s (%zu bytes)\n", filename, source_size);
    }

    /* Initialize lexer */
    LexerState *lex = nihao_malloc(cs, sizeof(LexerState));
    cs->parser.lex = lex;
    lexer_init(cs, filename, source);

    /* Dump the full token stream (lexer self-test mode) */
    int count = 0;
    for (;;) {
        lexer_next(lex);
        printf("%4d:%-3d  %-22s", lex->line_num, lex->col_num,
               token_name(lex->tok));
        if (lex->tok == TOK_INT_CONST)      printf("  = %lld", (long long)lex->tok_val.i);
        else if (lex->tok == TOK_FLOAT_CONST) printf("  = %g", lex->tok_val.f);
        else if (lex->tok == TOK_STRING_LITERAL || lex->tok == TOK_IDENTIFIER)
            printf("  = '%s'", lex->tok_str ? lex->tok_str : "");
        printf("\n");
        if (lex->tok == TOK_EOF || ++count > 1000) break;
    }
    printf("Total tokens: %d\n", count);
    return;
}

/* ============================================================
 * Sub-commands: init / build / run / debug
 * ============================================================ */

/* Compile file with the given option argv (argv[0]=prog, argv[1]=file).
 * Returns 0 on success. */
static int compile_argv(CompilerState *cs, int argc, char **argv)
{
    if (parse_args(cs, argc, argv) != 0) {
        return 1;
    }
    if (!cs->input_file) {
        fprintf(stderr, "Error: no input file specified\n");
        return 1;
    }
    if (!cs->output_file) {
        /* default: <input-stem> in current dir */
        const char *in = cs->input_file;
        const char *slash = strrchr(in, '/');
        const char *bslash = strrchr(in, '\\');
        const char *base = (bslash && (!slash || bslash > slash)) ? bslash + 1
                          : (slash ? slash + 1 : in);
        char buf[512];
        snprintf(buf, sizeof(buf), "%.*s", (int)strlen(base) - 2, base);
        cs->output_file = nihao_malloc(cs, strlen(buf) + 1);
        strcpy(cs->output_file, buf);
    }
    int ret = compile_file(cs, cs->input_file);
    return ret != 0 ? 1 : 0;
}

static int cmd_init(int argc, char **argv)
{
    const char *name = (argc >= 3) ? argv[2] : "myapp";
    char dir[512];
    char src_dir[512];

    snprintf(dir, sizeof(dir), "%s", name);
    snprintf(src_dir, sizeof(src_dir), "%s/src", name);

#ifdef _WIN32
    {
        char mk[1024];
        snprintf(mk, sizeof(mk), "if not exist \"%s\" mkdir \"%s\"", src_dir, src_dir);
        system(mk);
    }
#else
    {
        char mk[1024];
        snprintf(mk, sizeof(mk), "mkdir -p \"%s\"", src_dir);
        system(mk);
    }
#endif

    /* nihao.toml */
    char path[1024];
    FILE *fp;
    snprintf(path, sizeof(path), "%s/nihao.toml", dir);
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp,
            "[project]\n"
            "name = \"%s\"\n"
            "version = \"0.1.0\"\n"
            "compiler = \"nihao\"\n"
            "\n"
            "[build]\n"
            "output = \"bin/%s\"\n",
            name, name);
        fclose(fp);
    }

    /* src/main.nc */
    snprintf(path, sizeof(path), "%s/src/main.nc", dir);
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp,
            "module main\n"
            "use stdio\n"
            "\n"
            "func main() {\n"
            "    puts(\"hello from %s!\")\n"
            "}\n",
            name);
        fclose(fp);
    }

    /* .gitignore */
    snprintf(path, sizeof(path), "%s/.gitignore", dir);
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp, "bin/\n*.c\n*.exe\n");
        fclose(fp);
    }

    printf("Created project '%s':\n", name);
    printf("  %s/nihao.toml\n", name);
    printf("  %s/src/main.nc\n", name);
    printf("Next: nihao build %s/src/main.nc -o %s/bin/%s\n", name, name, name);
    return 0;
}

static int cmd_build(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: nihao build <file.nc> [options]\n");
        return 1;
    }
    /* 选项与文件可任意顺序：第一遍定位输入文件（第一个非选项、非选项值参数） */
    const char *input = NULL;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] == '-') {
            if (strcmp(a, "-o") == 0 || strcmp(a, "-backend") == 0 ||
                strcmp(a, "-I") == 0 || strcmp(a, "-L") == 0 ||
                strcmp(a, "-l") == 0 || strcmp(a, "--link") == 0) {
                i++;                    /* skip option value */
            }
            continue;
        }
        input = a;
        break;
    }
    if (!input) {
        fprintf(stderr, "Error: no input file specified\n");
        return 1;
    }
    /* 第二遍：nargv = prog, input, 其余参数原样透传 */
    CompilerState *cs = nihao_new();
    g_cs = cs;
    char **nargv = calloc(argc + 1, sizeof(char *));
    nargv[0] = argv[0];
    nargv[1] = (char *)input;
    int n = 2;
    for (int i = 2; i < argc; i++) {
        if (argv[i] == input) continue;
        nargv[n++] = argv[i];
    }
    int rc = compile_argv(cs, n, nargv);
    free(nargv);
    nihao_cleanup(cs);
    return rc;
}

static int cmd_run(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: nihao run <file.nc> [-- prog-args...]\n");
        return 1;
    }
    /* Split program args after '--' */
    int prog_argc = 0;
    char **prog_argv = NULL;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            prog_argc = argc - i - 1;
            prog_argv = argv + i + 1;
            break;
        }
    }

    CompilerState *cs = nihao_new();
    g_cs = cs;
    int n = 2;
    char **nargv = calloc(3, sizeof(char *));
    nargv[0] = argv[0];
    nargv[1] = argv[2];
    int rc = compile_argv(cs, n, nargv);
    free(nargv);
    if (rc != 0) {
        nihao_cleanup(cs);
        return 1;
    }
    const char *out = cs->output_file ? cs->output_file : "a.out";
    nihao_cleanup(cs);

    /* Run the produced executable */
    char cmd[1600];
    snprintf(cmd, sizeof(cmd), "\"%s\"", out);
    for (int i = 0; i < prog_argc; i++) {
        size_t len = strlen(cmd);
        snprintf(cmd + len, sizeof(cmd) - len, " \"%s\"", prog_argv[i]);
    }
    if (getenv("NIHAO_VERBOSE_RUN")) {
        printf("Running: %s\n", cmd);
    }
    return system(cmd);
}

static int cmd_debug(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: nihao debug <file.nc>\n");
        return 1;
    }
    CompilerState *cs = nihao_new();
    g_cs = cs;
    int n = 4;
    char **nargv = calloc(5, sizeof(char *));
    nargv[0] = argv[0];
    nargv[1] = argv[2];
    nargv[2] = "-v";
    nargv[3] = "-g";
    int rc = compile_argv(cs, n, nargv);
    if (rc == 0) {
        char cpath[1024];
        snprintf(cpath, sizeof(cpath), "%s.c",
                 cs->output_file ? cs->output_file : "a.out");
        printf("\n=== generated C (%s) ===\n", cpath);
        FILE *fp = fopen(cpath, "rb");
        if (fp) {
            char buf[4096];
            size_t rd;
            while ((rd = fread(buf, 1, sizeof(buf), fp)) > 0) {
                fwrite(buf, 1, rd, stdout);
            }
            fclose(fp);
        }
        printf("=== end C ===\n");
    }
    free(nargv);
    nihao_cleanup(cs);
    return rc;
}

/* ============================================================
 * Program Entry Point
 * ============================================================ */

int nihao_main(int argc, char **argv)
{
    /* Sub-command dispatch */
    if (argc >= 2) {
        const char *cmd = argv[1];
        if (strcmp(cmd, "init") == 0)   return cmd_init(argc, argv);
        if (strcmp(cmd, "build") == 0)  return cmd_build(argc, argv);
        if (strcmp(cmd, "run") == 0)    return cmd_run(argc, argv);
        if (strcmp(cmd, "debug") == 0)  return cmd_debug(argc, argv);
        if (strcmp(cmd, "lex") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Usage: nihao lex <file.nc>\n");
                return 1;
            }
            CompilerState *cs = nihao_new();
            g_cs = cs;
            lexer_test(cs, argv[2]);
            nihao_cleanup(cs);
            return 0;
        }
        if (strcmp(cmd, "test") == 0) {
            fprintf(stderr, "Use tests/run_tests.py instead.\n");
            return 1;
        }
        /* fall through to legacy behavior */
    }

    CompilerState *cs;
    int ret;

    /* Create compiler instance */
    cs = nihao_new();
    g_cs = cs;

    /* Parse command line arguments */
    if (parse_args(cs, argc, argv) != 0) {
        nihao_cleanup(cs);
        return 1;
    }

    if (cs->verbose) {
        printf("NihaoC Compiler v" NIHAO_VERSION "\n");
        printf("Input:  %s\n", cs->input_file);
        printf("Output: %s\n", cs->output_file);
    }

    /* Compile input file (skip in -lexertest mode) */
    if (cs->test_mode) {
        nihao_cleanup(cs);
        return 0;
    }
    ret = compile_file(cs, cs->input_file);

    /* Cleanup */
    nihao_cleanup(cs);

    return ret != 0 ? 1 : 0;
}

/* Standard main */
int main(int argc, char **argv)
{
    return nihao_main(argc, argv);
}
