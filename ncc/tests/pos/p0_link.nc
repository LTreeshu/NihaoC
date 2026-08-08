module main

/* 静态库导入声明：link "lib" alias / link "lib" as alias
 * （解析记录到 link_libs；实际 -l 链接为规划特性，无外部库可测） */
link "libhttp.so" http
link "libm.so" as libm
link "libc.so"

func main() {
    puts("link ok")
    return
}
