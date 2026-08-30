#include "ncc.h"

/* ============================================================
 * Linker — 模块/库管理（1.0 文档化保留）
 *
 * 职责：
 *   - linker_init：初始化链接状态（link_lib_count）
 *   - link_add_library：记录 `link` 指令声明的链接库（name/alias/path）
 *
 * 现状（1.0 定位）：
 *   A 方案产品路径为 parser → C 文本 → tcc（cgen.c），本模块仅承担
 *   `link` 指令的库声明收集（stdlib_resolve_link_libraries 消费）；
 *   实际链接由 tcc 完成。早期"直接 native 代码生成"遗留路径
 *   （codegen.c 字节发射 + linker_generate_* raw binary 输出）已于
 *   2026-09-01 删除（见 docs/LEGACY_CODEGEN.md）。
 * ============================================================ */

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
