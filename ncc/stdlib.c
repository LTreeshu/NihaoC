#include "ncc.h"

/* ============================================================
 * Standard Library Registration
 * ============================================================ */

void stdlib_register_all(CompilerState *cs)
{
    /* Register all built-in standard library modules and functions.
     *
     * The NihaoC standard library includes:
     * - stdio: input/output functions (puts, printf, scanf, etc.)
     * - stdlib: memory allocation, conversion functions
     * - string: string manipulation
     * - math: mathematical functions
     * - sys: system calls and OS interaction
     * - memory: low-level memory operations
     *
     * Each module provides function declarations that map to
     * either compiler built-ins or external C library calls.
     */

    if (cs->verbose) {
        printf("Registering standard library...\n");
    }

    /* Built-in functions are already registered in sym_register_builtins.
     * Here we register additional standard library modules that can be
     * imported with 'use' statements.
     */

    /* Register stdio module */
    Module *stdio_mod = module_add(cs, "stdio", NULL);
    if (stdio_mod) {
        stdio_mod->is_external = 1;
    }

    /* Register stdlib module */
    Module *stdlib_mod = module_add(cs, "stdlib", NULL);
    if (stdlib_mod) {
        stdlib_mod->is_external = 1;
    }

    /* Register string module */
    Module *string_mod = module_add(cs, "string", NULL);
    if (string_mod) {
        string_mod->is_external = 1;
    }

    /* Register math module */
    Module *math_mod = module_add(cs, "math", NULL);
    if (math_mod) {
        math_mod->is_external = 1;
    }

    /* Register memory module */
    Module *memory_mod = module_add(cs, "memory", NULL);
    if (memory_mod) {
        memory_mod->is_external = 1;
    }

    /* Register sys module */
    Module *sys_mod = module_add(cs, "sys", NULL);
    if (sys_mod) {
        sys_mod->is_external = 1;
    }

    if (cs->verbose) {
        printf("  Registered %d standard library modules\n", cs->module_count);
    }
}

/* ============================================================
 * Link Library Resolution
 * ============================================================ */

void stdlib_resolve_link_libraries(CompilerState *cs)
{
    /* Resolve all linked libraries to their actual file paths.
     *
     * This function:
     * 1. Searches library paths for each linked library
     * 2. Validates that library files exist
     * 3. Determines library type (static .a / dynamic .so/.dll)
     * 4. Extracts symbol information from libraries
     * 5. Sets up import tables for dynamic libraries
     */

    if (cs->verbose) {
        printf("Resolving link libraries...\n");
    }

    for (int i = 0; i < cs->link_lib_count; i++) {
        LinkLib *lib = &cs->link_libs[i];

        if (cs->verbose) {
            printf("  Library: %s (alias: %s)\n",
                   lib->name ? lib->name : "(null)",
                   lib->alias ? lib->alias : "(none)");
        }

        /* TODO: Actually resolve library paths
         * Search in:
         * - Current directory
         * - -L paths from command line
         * - Standard library paths (/usr/lib, /usr/local/lib, etc.)
         * - Platform-specific locations
         */

        /* Check if file exists directly */
        FILE *fp = fopen(lib->path, "rb");
        if (fp) {
            fclose(fp);
            if (cs->verbose) {
                printf("    Found at: %s\n", lib->path);
            }
        } else {
            nihao_warning(cs, "library '%s' not found, will be treated as external",
                         lib->name ? lib->name : "(null)");
        }
    }
}

/* ============================================================
 * Runtime Stub Generation
 * ============================================================ */

void stdlib_generate_runtime_stubs(CompilerState *cs)
{
    /* Generate runtime support stubs that are linked into every program.
     *
     * Runtime stubs include:
     * - _start entry point (calls main and handles exit)
     * - Built-in function implementations (puts, printf wrappers, etc.)
     * - Memory allocator support
     * - Exception handling infrastructure
     * - Garbage collection support (for flow-allocated objects)
     *
     * For external C library functions, we generate PLT entries
     * or direct import symbols.
     */

    (void)cs;

    if (cs->verbose) {
        printf("Generating runtime stubs...\n");
    }

    /* TODO: Generate _start entry point
     *
     * The _start function would:
     * 1. Set up the initial stack frame
     * 2. Call main()
     * 3. Call exit() with the return value
     *
     * For now, we rely on the system C runtime (crt0)
     * and just ensure main is properly defined.
     */

    /* Generate built-in function stubs that call libc equivalents.
     * These would be actual function bodies that:
     * - puts: call libc puts
     * - printf: call libc printf
     * - malloc: call libc malloc
     * - etc.
     *
     * Since we're generating raw machine code, these stubs would
     * be proper function implementations. For now, we mark them
     * as external symbols to be resolved by the system linker.
     */

    if (cs->verbose) {
        printf("  Runtime stubs: using external C library (placeholder)\n");
        printf("  Note: Full runtime code generation is not yet implemented.\n");
        printf("        Programs must be linked with libc for standard functions.\n");
    }
}
