module main

/* BNF §8 <cooking-item> 的首项 <var-decl> 在 A 后端既不支持也无诊断：
 * `cooking { K i32 = 3 }` 此前整条静默丢弃，引用 K 时才由 tcc 报 'K' undeclared。
 * 现即时报 unsupported cooking item（PA-59） */
cooking {
    K i32 = 3
}

func main() {
    print("%d\n", K)
    return
}
