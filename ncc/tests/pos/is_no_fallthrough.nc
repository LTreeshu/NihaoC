module main
use stdio

func main() {
    /* §6.1：多个 is-clause 按源码顺序求值，首个匹配者执行（无 fallthrough）。
     * n = 3 同时命中 `is 3` 与其后的 `is 1..5`，只应输出 "only three"；
     * 其余取值不命中前子句时后续子句照常执行，故标记须在每轮迭代重置。 */
    n i32 = 0
    while n += 1 {
        is 3 {
            puts("only three")
        }
        is 1..5 {
            puts("range")
        }
        is _ {
            puts("wild")
        }
        if n >= 6 {
            break
        }
    }
    puts("no fallthrough ok")
}
