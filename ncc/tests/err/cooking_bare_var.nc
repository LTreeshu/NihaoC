module main

/* BNF §8 <cooking-item> 的首个形态 <var-decl> 在 A 后端此前既不落地也不报错：
 * `cooking { K i32 = 3 }` 被逐 token 丢弃，直到引用 K 时才由 tcc 报 'K' undeclared。
 * 现即时报 unsupported cooking item，并只跳过该 item、不吞后续 item。 */
cooking {
    K i32 = 3
}

func main() {
    print("%d\n", K)
    return
}
