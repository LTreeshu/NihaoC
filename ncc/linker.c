#include "ncc.h"

/* ============================================================
 * Linker — 模块/库管理（1.0 文档化保留）
 *
 * 职责：
 *   - linker_init：初始化链接状态（link_lib_count），由 ncc.c 主流程调用
 *   - link_add_library：记录 `link` 指令声明的链接库（name/alias/path）
 *
 * 与后端的关系（2026-09-05 梳理，P3）：
 *   本模块是"库声明收集器"，不参与实际链接代码的生成；真正的链接
 *   由外部 tcc 完成。调用/消费关系如下：
 *
 *   ① 调用方（写入 link_libs）：
 *        - parser.c（A 方案 / 默认后端链路 parser→cgen）：解析 `link`
 *          指令时调 link_add_library（唯一现役写入点）。
 *        - ncc.c:346 的 CLI 直接声明入口已注释停用（改由文件内 `link` 指令）。
 *      ⚠ IR 后端链路（irparse.c → ir_to_c.c / ir_to_native.c）当前【不】
 *        调用本模块——`link` 指令在 IR 双后端下尚未生效；该能力属 2.0
 *        阶段 2「link/use 跨文件」项（路线图 A），届时再接入 irparse。
 *
 *   ② 消费方（读取 link_libs，交给 tcc）：
 *        - native.c:144  遍历 g_cs->link_libs 调 tcc_add_library（native 后端）
 *        - stdlib.c:94    stdlib_resolve_link_libraries 解析供 c 后端 -l 传递
 *      c 后端实际链接由 tcc 完成（cgen 生成 .c 后外部 tcc 编译）。
 *
 *   结论：linker.c 目前仅服务于 A/默认后端（parser→cgen）链路，
 *   IR 后端暂未接入（留待 2.0 阶段 2 跨文件/link 项）。
 *
 * 历史：早期"直接 native 代码生成"遗留路径（codegen.c 字节发射 +
 *   linker_generate_* raw binary 输出）已于 2026-09-01 删除
 *   （见 docs/LEGACY_CODEGEN.md）。
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
