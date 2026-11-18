module main

/* BNF §7 <bracket-attr> ::= "local" | "inline" | "weak" | "used" | "unused"
 *   | "export" [ <string-literal> ]：六个名字之外没有扩展位。改前未知属性被静默吞掉
 *   （既不落地也不报错，函数照常出码），现两个前端各报一条专属诊断并整条消费。 */
[[nope]] func hidden() i32 {
    return 1
}

func main() {
    puts("unreachable")
}
