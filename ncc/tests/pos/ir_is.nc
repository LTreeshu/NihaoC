module main
func main() {
    /* 负数模式：is -1 匹配循环条件值 == -1 */
    a i32 = 0
    while a -= 1 {
        is -1 {
            puts("is neg ok")
            break
        }
    }
    /* 区间模式：is 0..2 闭区间匹配 */
    b i32 = 3
    while b -= 1 {
        is 0..2 {
            puts("is range ok")
            break
        }
    }
    /* 可见性模式：is _flow 匹配 visof() 查询结果（VIS_FLOW=2） */
    flow v i32 = 1
    while visof(v) {
        is _flow {
            puts("is vis ok")
            break
        }
    }
    /* 负数模式（块形式；规范 2026-09-01 移除 => 箭头形式，此处以块形式保留覆盖） */
    c i32 = 0
    while c -= 1 {
        is -1 {
            puts("is neg2 ok")
            break
        }
    }
    /* 区间模式（块形式） */
    d i32 = 2
    while d -= 1 {
        is 0..2 {
            puts("is range2 ok")
            break
        }
    }
    return
}
