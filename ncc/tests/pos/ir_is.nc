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
    /* 单语句形式：is pat => stmt（BNF <is-stmt>）——负数模式 */
    c i32 = 0
    while c -= 1 {
        is -1 => puts("is arrow ok")
        break
    }
    /* 单语句形式：区间模式 */
    d i32 = 2
    while d -= 1 {
        is 0..2 => puts("is arrow range ok")
        break
    }
    return
}
