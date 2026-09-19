module main
func main() {
    v i32 = 0
    do v < 3 {
        is 1 {
            puts("bad")
        }
    }
    /* 嵌套情形：外层 while 有条件值，do 体内仍不得用 is（不得静默匹配外层值） */
    while v < 3 {
        do v < 2 {
            is 2 {
                puts("bad nested")
            }
        }
    }
}
