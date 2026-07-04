#include "ncc.h"

CompilerState *g_cs;

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
        "Usage: nihao [options] <input-file>\n"
        "\n"
        "Options:\n"
        "  -o <file>       Output file name\n"
        "  -c              Compile only (object file)\n"
        "  -shared         Generate shared library\n"
        "  -static         Generate static library\n"
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
        "  nihao hello.nh -o hello\n"
        "  nihao -c module.nh -o module.o\n"
        "  nihao --link libhttp.so as http -o program main.nh\n"
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

static char *load_source_file(const char *filename, size_t *size_out)
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

    /* Load source file */
    source = load_source_file(filename, &source_size);
    if (!source) return -1;

    if (cs->verbose) {
        printf("Compiling: %s (%zu bytes)\n", filename, source_size);
    }

    // /* Initialize lexer */
    // LexerState *lex = nihao_malloc(cs, sizeof(LexerState));
    // cs->parser.lex = lex;
    // lexer_init(cs, filename, source);

    // /* Initialize parser */
    // parser_init(cs);

    // /* Parse module */
    // parse_module(cs);

    // /* Check for errors */
    // if (cs->error_count > 0) {
    //     fprintf(stderr, "Compilation failed with %d error(s), %d warning(s)\n",
    //             cs->error_count, cs->warning_count);
    //     return -1;
    // }

    // if (cs->verbose) {
    //     printf("Compilation successful (%d warnings)\n", cs->warning_count);
    // }

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

    return;
}

/* ============================================================
 * Program Entry Point
 * ============================================================ */

int nihao_main(int argc, char **argv)
{
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

    /* Compile input file */
    // ret = compile_file(cs, cs->input_file);

    // if (ret == 0 && cs->output_type == 0) {
    //     /* Generate executable (link phase) */
    //     if (cs->verbose) printf("Linking...\n");
    //     linker_generate_executable(cs, cs->output_file);
    // }

    // /* Cleanup */
    // nihao_cleanup(cs);

    return ret != 0 ? 1 : 0;
}

/* Standard main */
int main(int argc, char **argv)
{
    return nihao_main(argc, argv);
}
