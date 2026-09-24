module main

/* BNF §2.1 <top-level> 含 <linkas-decl> ::= "linkas" <string-literal>，
 * 1.x 无任何消费者（模块系统属 2.0）：整条声明消费后报专属诊断，
 * 不再退化为两条通用的 'unexpected token ... at declaration level' */
linkas "libc.so"

func main() {
    print("%d\n", 1)
    return
}
