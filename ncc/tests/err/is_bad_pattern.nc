module main
use stdio

/* BNF §6 <pattern> 之外的畸形形态：符号后不接整数字面量。
   `is -x` 改前被静默按 `is x` 出码，`is -` 改前在诊断后继续展开子句而级联多条错；
   现两前端都对每条畸形形态报一条错，并把该子句整条丢弃（后续子句仍可解析）。 */

func main() {
    x i32 = 2
    n i32 = 9
    while n -= 1 {
        is -x {
            puts("bad minus ident")
        }
        is 8 {
            puts("recover after -x")
            break
        }
    }
    while n -= 1 {
        is - {
            puts("bad dangling minus")
        }
        is 6 {
            puts("recover after dangling")
            break
        }
    }
    while n -= 1 {
        is "s" {
            puts("bad string pattern")
        }
        is 4 {
            puts("recover after string")
            break
        }
    }
    return
}
