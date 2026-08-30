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

/* MSYS/Git-Bash 风格路径（/d/foo）转 Windows 盘符路径（D:/foo）。
 * Windows 程序无法解析 /d/ 前缀，若 NIHAO_TCC_DIR 由 bash 传入需归一化。 */
static void normalize_msys_path(char *out, size_t outsz, const char *in)
{
    if (in && in[0] == '/' && in[1] != '\0' && in[2] == '/' && in[1] != '/') {
        if (outsz > 4) {
            out[0] = (char)toupper((unsigned char)in[1]);
            out[1] = ':';
            out[2] = '/';
            snprintf(out + 3, outsz - 3, "%s", in + 3);
            return;
        }
    }
    snprintf(out, outsz, "%s", in ? in : "");
}

static const char *tcc_install_dir(void)
{
    static char dir[512];
    static int resolved = 0;
    if (!resolved) {
        resolved = 1;
        const char *env = getenv("NIHAO_TCC_DIR");
        if (env && env[0]) {
            normalize_msys_path(dir, sizeof(dir), env);
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
                        normalize_msys_path(dir, sizeof(dir), buf);
                        return dir;
                    }
                    snprintf(probe, sizeof(probe), "%s/tcc", buf);
                    f = fopen(probe, "rb");
                    if (f) {
                        fclose(f);
                        normalize_msys_path(dir, sizeof(dir), buf);
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

/* libtcc 错误回调：把编译器错误整合进 nihao 错误输出（PA-8） */
static void nihao_tcc_error(void *opaque, const char *msg)
{
    (void)opaque;
    fprintf(stderr, "native: %s\n", msg ? msg : "(unknown error)");
}

/* 配置共享的 libtcc 状态 */
static TCCState *native_state(const char *csrc, int output_type, int verbose, int debug)
{
    TCCState *s = tcc_new();
    if (!s) {
        fprintf(stderr, "native: tcc_new failed\n");
        return NULL;
    }

    tcc_set_error_func(s, NULL, nihao_tcc_error);

    char libpath[1024];
    char incpath[1024];
#ifdef _WIN32
    const char *dir = tcc_install_dir();
    snprintf(libpath, sizeof(libpath), "%s/lib", dir);
    snprintf(incpath, sizeof(incpath), "%s/include", dir);
#else
    /* Linux 源码构建的 libtcc.so 未导出 tcc_install_dir
     * （2026-08-31 实测：隐式声明→ int 截断指针→ segfault）。
     * CONFIG_TCCDIR 由 install: /usr/local/lib/tcc
include:
  /usr/local/lib/tcc/include
  /usr/local/include/x86_64-linux-gnu
  /usr/local/include
  /usr/include/x86_64-linux-gnu
  /usr/include
libraries:
  /usr/lib/x86_64-linux-gnu
  /usr/lib
  /lib/x86_64-linux-gnu
  /lib
  /usr/local/lib/x86_64-linux-gnu
  /usr/local/lib
libtcc1:
  /usr/local/lib/tcc/libtcc1.a
crt:
  /usr/lib/x86_64-linux-gnu
elfinterp:
  /lib64/ld-linux-x86-64.so.2 确认 = /usr/local/lib/tcc：
     * libtcc1.a 在其根目录，自带头在 include/ 子目录。 */
    const char *dir = "/usr/local/lib/tcc";
    snprintf(libpath, sizeof(libpath), "%s", dir);
    snprintf(incpath, sizeof(incpath), "%s/include", dir);
#endif

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
    /* link 库声明（PA-7）：nihao link "lib" 别名 -> libtcc 链接 */
    if (g_cs) {
        for (int i = 0; i < g_cs->link_lib_count; i++) {
            LinkLib *ll = &g_cs->link_libs[i];
            if (ll->name && ll->name[0]) {
                tcc_add_library(s, ll->name);
            }
        }
    }
    {
        char opts[64] = "-Wall";
        if (debug) strcat(opts, " -g");
        if (verbose) {
            tcc_set_options(s, opts);
        }
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
int native_compile_string(const char *csrc, const char *outfile, int verbose, int debug)
{
    TCCState *s = native_state(csrc, TCC_OUTPUT_EXE, verbose, debug);
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
int native_run_string(const char *csrc, int argc, char **argv, int verbose, int debug)
{
    TCCState *s = native_state(csrc, TCC_OUTPUT_MEMORY, verbose, debug);
    if (!s) return -1;
    /* ⚠️ 不要先调用 tcc_relocate(s, NULL)：
     * NULL 语义是"返回所需内存大小"（>0 为成功，仅 -1 为错误），
     * 且 libtcc.h 明确 tcc_run 之前 DO NOT call tcc_relocate。
     * 直接 tcc_run——其内部自行 relocate（TCC_RELOCATE_AUTO）后进入 main。
     * （2026-08-31 WSL 实测：tcc_relocate(NULL) 返回 223 被误判失败，
     *   Linux 上 -run 全部报 relocation failed） */
    int rc = tcc_run(s, argc, argv);
    tcc_delete(s);
    return rc;
}

/* 后端可用性：libtcc 静态链接，EXE 输出总是可用 */
int native_backend_available(void)
{
    return 1;
}

/* 内存执行（-run）可用性：Windows libtcc 0.9.27 的 TCC_OUTPUT_MEMORY 损坏（PA-1/PA-10） */
int native_memory_available(void)
{
#ifdef _WIN32
    return 0;
#else
    return 1;
#endif
}
