module main

/* BNF §2.1 <top-level> 含 <linkas-decl> ::= "linkas" <string-literal>：本分支（2.0 线）
 * 目前没有任何消费者（静态库导出命名要随模块系统一起落地），但它是**要实现的范围**，
 * 不是永久出局。改前整条落进顶层通用分支，连报两条
 * 'unexpected token ... at declaration level'，第二条还把 token 枚举名直接透出。
 * 现整条消费并给一条专属诊断。 */
linkas "libc.so"

func main() {
    print("%d\n", 1)
    return
}
