module main
use stdio

/* BNF §6 <pattern>：整数字面量（含负数与带符号）、闭区间（含负区间）、枚举变体、
   裸标识符按值比较、通配符 `_`；多个 is 按源码顺序求值，首个匹配者执行且只执行一个 */

Level enum { LOW = 1, MID = 5, HIGH = 9 }

/* 返回值标出命中的是哪一条 is：1 负字面量 / 2 带符号字面量 / 3 负区间
   4 枚举变体 / 5 标识符按值 / 6 通配符 / 0 循环未进入 */
func pick(v i32, want i32) i32 {
    hit i32 = 0
    n i32 = v
    while n {
        is -3 {
            hit = 1
            break
        }
        is +5 {
            hit = 2
            break
        }
        is -9..-5 {
            hit = 3
            break
        }
        is LOW {
            hit = 4
            break
        }
        is want {
            hit = 5
            break
        }
        is _ {
            hit = 6
            break
        }
    }
    return hit
}

func main() {
    if pick(-3, 7) == 1 { puts("is negative lit ok") } else { puts("is negative lit bad") }
    if pick(5, 7) == 2 { puts("is signed lit ok") } else { puts("is signed lit bad") }
    if pick(-7, 7) == 3 { puts("is negative range ok") } else { puts("is negative range bad") }
    if pick(1, 7) == 4 { puts("is enum variant ok") } else { puts("is enum variant bad") }
    if pick(7, 7) == 5 { puts("is ident by value ok") } else { puts("is ident by value bad") }
    if pick(4, 7) == 6 { puts("is wildcard ok") } else { puts("is wildcard bad") }
    /* 首个匹配者独占：5 同时命中 `is +5`、`is MID`（枚举值 5）与 `is _`，只执行第一条 */
    if pick(5, 5) == 2 { puts("is first match only ok") } else { puts("is first match only bad") }
    return
}
