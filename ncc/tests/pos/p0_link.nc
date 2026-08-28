module main

/* 静态库导入声明：link "lib" alias / link "lib" as alias / link "lib"
 * 解析记录到 link_libs；c 后端实际 -l 传递（2026-08-28 完成），
 * native 后端 tcc_add_library（PA-7）。测试用 Windows 系统库 kernel32（tcc 自带 .def） */
link "kernel32" http
link "kernel32" as libk32
link "kernel32"

func main() {
    puts("link ok")
    return
}
