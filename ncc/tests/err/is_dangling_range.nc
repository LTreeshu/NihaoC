module main
use stdio

/* BNF §6 <pattern> 的闭区间要求上界为整数字面量：
   `is 5..` 与 `is 5..x` 都只报一条诊断，并丢弃该子句。 */

func main() {
    x i32 = 2
    n i32 = 9
    while n -= 1 {
        is 5.. {
            puts("bad dangling range")
        }
        is 8 {
            puts("recover after dangling")
            break
        }
    }
    while n -= 1 {
        is 5..x {
            puts("bad ident upper bound")
        }
        is 6 {
            puts("recover after ident bound")
            break
        }
    }
    return
}
