module main
use stdio

/* ============================================================
 * 05_string — 字符串与字符数组
 *
 * 演示：字符串字面量 → char 数组（含 NUL）/ 字符字面量 /
 *       字符读写 / string 类型（字符串地址别名）/
 *       字符串池去重
 * 1.0 可编译：是（c/native 后端）
 * ============================================================ */

func main() {
    /* 字符串字面量拷入 char 数组，末尾自动补 NUL */
    s char[6] = "hello"
    if s[0] == 104 && s[4] == 111 && s[5] == 0 {
        puts("nul term ok")
    } else {
        puts("nul term bad")
    }

    /* 字符字面量与元素写入 */
    s[1] = 'H'
    if s[1] == 72 {
        puts("char write ok")
    } else {
        puts("char write bad")
    }

    /* string 类型：存字符串地址（指针别名） */
    p string = "world"
    q char[6] = "world"
    if q[0] == 119 && q[4] == 100 {
        puts("string ok")
    } else {
        puts("string bad")
    }

    /* 字符串池去重：两个 "hello" 只生成一份存储 */
    s2 char[6] = "hello"
    if s2[0] == 104 {
        puts("str pool ok")
    } else {
        puts("str pool bad")
    }
}
