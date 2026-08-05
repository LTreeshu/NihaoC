/* ============================================================
 * native.c - libtcc 进程内机器码后端 (-backend=native)
 *
 * 将 cgen 生成的 C 文本直接交给 libtcc 在进程内编译为机器码：
 *   - native_compile_string : 生成可执行文件 (TCC_OUTPUT_EXE)
 *   - native_run_string     : 编译到内存并直接执行 main (TCC_OUTPUT_MEMORY)
 *
 * 依赖：libtcc (tcc 0.9.27 自带 libtcc.h / libtcc.dll)
 *       安装目录探测顺序: NIHAO_TCC_DIR > PATH 中 tcc.exe 所在目录
 * ============================================================ */
#include "ncc.h"
#include <libtcc.h>

static const char *tcc_install_dir(void)
{
    static char dir[512];
    static int resolved = 0;
    if (!resolved) {
        resolved = 1;
        const char *env = getenv("NIHAO_TCC_DIR");
        if (env && env[0]) {
            snprintf(dir, sizeof(dir), "%s", env);
            return dir;
        }
        /* probe PATH for tcc.exe / tcc */
        const char *path = getenv("PATH");
        if (path) {
            char buf[1024];
            const char *p = path;
            while (*p) {
                const char *sep = strchr(p, ';');
                size_t n = sep ? (size_t)(sep - p) : strlen(p);
                if (n > 0 && n < sizeof(buf)) {
                    memcpy(buf, p, n);
                    buf[n] = '\0';
                    char probe[1024];
                    snprintf(probe, sizeof(probe), "%s/tcc.exe", buf);
                    FILE *f = fopen(probe, "rb");
                    if (f) {
                        fclose(f);
                        snprintf(dir, sizeof(dir), "%s", buf);
                        return dir;
                    }
                    snprintf(probe, sizeof(probe), "%s/tcc", buf);
                    f = fopen(probe, "rb");
                    if (f) {
                        fclose(f);
                        snprintf(dir, sizeof(dir), "%s", buf);
                        return dir;
                    }
                }
                if (!sep) break;
                p = sep + 1;
            }
        }
        strcpy(dir, ".");
    }
    return dir;
}

/* 配置共享的 libtcc 状态 */
static TCCState *native_state(const char *csrc, int output_type, int verbose)
{
    TCCState *s = tcc_new();
    if (!s) {
        fprintf(stderr, "native: tcc_new failed\n");
        return NULL;
    }

    const char *dir = tcc_install_dir();
    char libpath[1024];
    char incpath[1024];
    snprintf(libpath, sizeof(libpath), "%s/lib", dir);
    snprintf(incpath, sizeof(incpath), "%s/include", dir);

    tcc_set_lib_path(s, libpath);
    tcc_add_include_path(s, incpath);
    tcc_add_sysinclude_path(s, incpath);
    tcc_add_library_path(s, libpath);
#ifdef _WIN32
    /* In-memory output needs the CRT symbols (printf etc.) resolved at
     * relocate time; the MSVCRT import library lives next to libtcc1. */
    if (output_type == TCC_OUTPUT_MEMORY) {
        tcc_add_library(s, "msvcrt");
    }
#endif
    if (verbose) {
        tcc_set_options(s, "-Wall");
    }
    tcc_set_output_type(s, output_type);

    if (tcc_compile_string(s, csrc) != 0) {
        fprintf(stderr, "native: compilation of generated C failed\n");
        tcc_delete(s);
        return NULL;
    }
    return s;
}

/* 编译 C 文本为可执行文件（写 <outfile>） */
int native_compile_string(const char *csrc, const char *outfile, int verbose)
{
    TCCState *s = native_state(csrc, TCC_OUTPUT_EXE, verbose);
    if (!s) return -1;
    if (tcc_output_file(s, outfile) != 0) {
        fprintf(stderr, "native: failed to write executable '%s'\n", outfile);
        tcc_delete(s);
        return -1;
    }
    tcc_delete(s);
    return 0;
}

/* 编译 C 文本到内存并执行 main(argc, argv) */
int native_run_string(const char *csrc, int argc, char **argv, int verbose)
{
    TCCState *s = native_state(csrc, TCC_OUTPUT_MEMORY, verbose);
    if (!s) return -1;
    if (tcc_relocate(s, NULL) != 0) {
        fprintf(stderr, "native: relocation failed\n");
        tcc_delete(s);
        return -1;
    }
    int rc = tcc_run(s, argc, argv);
    tcc_delete(s);
    return rc;
}

/* 后端可用性：libtcc 静态链接，总是可用 */
int native_backend_available(void)
{
    return 1;
}
