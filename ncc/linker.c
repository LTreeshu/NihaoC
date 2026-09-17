#include "ncc.h"

/* ============================================================
 * Linker — 模块/库管理（1.0 文档化保留）
 *
 * 职责：
 *   - linker_init：初始化链接状态（link_lib_count）
 *   - link_add_library：记录 `link` 指令声明的链接库（name/alias/path）
 *
 * 现状（1.0 定位）：
 *   A 方案产品路径为 parser → C 文本 → tcc（cgen.c），本模块仅负责
 *   收集 `link` 指令声明的库，交由 native.c/stdlib.c 消费；
 *   linker_generate_object / linker_generate_executable_full 属早期
 *   "直接 native 代码生成"遗留（依赖已删除的 codegen.c 的 Section/
 *   codegen 字段），已随 codegen 清理一并移除。完整链接能力留待
 *   后续（系统链接器或 ELF/PE 实现）。
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
