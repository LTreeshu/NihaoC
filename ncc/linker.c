#include "ncc.h"

/* ============================================================
 * Linker Initialization
 * ============================================================ */

void linker_init(CompilerState *cs)
{
    /* Initialize linker state.
     * In a full implementation, this would:
     * - Set up section headers
     * - Initialize symbol table for linking
     * - Prepare relocation entries
     * - Set up ELF/PE file headers
     */

    cs->link_lib_count = 0;

    if (cs->verbose) {
        printf("Linker initialized\n");
    }
}

/* ============================================================
 * Library Management
 * ============================================================ */

void link_add_library(CompilerState *cs, char *path, char *alias, char *lib_path)
{
    LinkLib *lib;

    if (cs->link_lib_count >= MAX_LINK_LIBS) {
        nihao_error(cs, "too many linked libraries (max %d)", MAX_LINK_LIBS);
        return;
    }

    lib = &cs->link_libs[cs->link_lib_count++];
    memset(lib, 0, sizeof(LinkLib));

    lib->name = path ? nihao_strdup(cs, path) : NULL;
    lib->alias = alias ? nihao_strdup(cs, alias) : NULL;
    lib->path = lib_path ? nihao_strdup(cs, lib_path) : NULL;
    lib->is_static = 0; /* default: dynamic */

    if (cs->verbose) {
        printf("Linked library: %s (alias: %s)\n",
               lib->name ? lib->name : "(null)",
               lib->alias ? lib->alias : "(none)");
    }
}

/* ============================================================
 * Object File Generation
 * ============================================================ */

void linker_generate_object(CompilerState *cs, char *output_file)
{
    FILE *fp;
    Section *sec;

    if (!output_file) return;

    if (cs->verbose) {
        printf("Generating object file: %s\n", output_file);
    }

    fp = fopen(output_file, "wb");
    if (!fp) {
        nihao_error(cs, "cannot open output file '%s'", output_file);
        return;
    }

    /* Write a minimal object file.
     * A full implementation would generate proper ELF .o or COFF .obj files
     * with section headers, symbol tables, and relocation entries.
     *
     * For now, we write the raw .text section data as a simple binary blob.
     * This is sufficient for testing the code generation pipeline.
     */

    sec = cs->codegen.text_section;
    if (sec && sec->data && sec->data_size > 0) {
        fwrite(sec->data, 1, sec->data_size, fp);
    }

    fclose(fp);

    if (cs->verbose) {
        printf("Object file generated: %d bytes of code\n",
               sec ? sec->data_size : 0);
    }
}

/* ============================================================
 * Executable Generation (Full Link)
 * ============================================================ */

void linker_generate_executable_full(CompilerState *cs, char *output_file)
{
    FILE *fp;

    if (!output_file) return;

    if (cs->verbose) {
        printf("Linking executable: %s\n", output_file);
        printf("  Modules: %d\n", cs->module_count);
        printf("  Linked libraries: %d\n", cs->link_lib_count);
    }

    /* Full executable generation would:
     * 1. Collect all object files and modules
     * 2. Resolve external symbols between modules
     * 3. Process relocations
     * 4. Lay out sections at final virtual addresses
     * 5. Generate program headers (ELF) or PE headers
     * 6. Write the final executable
     * 7. Set executable permissions
     *
     * For now, we generate a raw binary output as a placeholder.
     * A proper implementation would use system linker (ld) or
     * implement full ELF/PE format output.
     */

    fp = fopen(output_file, "wb");
    if (!fp) {
        nihao_error(cs, "cannot open output file '%s'", output_file);
        return;
    }

    /* Write combined text + data sections */
    Section *text = cs->codegen.text_section;
    Section *data = cs->codegen.data_section;
    Section *rodata = cs->codegen.rodata_section;

    if (text && text->data) {
        fwrite(text->data, 1, text->data_size, fp);
    }
    if (rodata && rodata->data) {
        fwrite(rodata->data, 1, rodata->data_size, fp);
    }
    if (data && data->data) {
        fwrite(data->data, 1, data->data_size, fp);
    }

    fclose(fp);

    if (cs->verbose) {
        int total = (text ? text->data_size : 0)
                  + (data ? data->data_size : 0)
                  + (rodata ? rodata->data_size : 0);
        printf("Executable generated: %d bytes total (raw binary)\n", total);
        printf("Note: This is a raw binary output, not a proper executable format.\n");
        printf("      Full ELF/PE support is not yet implemented.\n");
    }
}
