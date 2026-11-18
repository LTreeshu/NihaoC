module main
use stdio

/* BNF §6 <pattern> 的模式档位复测：可见性枚举（_const/_static/_var/_flow/_undef）、
   枚举常量、标识符按值、整数（含负数与闭区间）、通配符 _ 兜底。
   本分支 2026-10-05 实测四个后端逐档输出一致；首个匹配即止另由
   pos/is_no_fallthrough.nc 固定（IR 前端不做首匹配，故那份不入 IR 名单）。 */

Color enum { RED = 1, GREEN = 5, BLUE = 9 }

func main() {
    const c i32 = 10
    static st i32 = 20
    var v i32 = 30
    flow f i32 = 40

    while visof(c) {
        is _const {
            puts("vis const ok")
            break
        }
        is _ {
            puts("vis const bad")
        }
    }
    while visof(st) {
        is _static {
            puts("vis static ok")
            break
        }
        is _ {
            puts("vis static bad")
        }
    }
    while visof(v) {
        is _var {
            puts("vis var ok")
            break
        }
        is _ {
            puts("vis var bad")
        }
    }
    /* 首档不命中时由通配符兜底 */
    while visof(f) {
        is _const {
            puts("vis flow bad")
        }
        is _ {
            puts("vis wild ok")
            break
        }
    }
    /* 未声明标识符按 _undef 折叠为 0，while 条件即为假——子句不会执行 */
    while visof(nope) {
        is _undef {
            puts("vis undef bad")
            break
        }
    }
    puts("vis undef end")

    q i32 = 0
    while q -= 1 {
        is -1 {
            puts("pat neg ok")
            break
        }
        is _ {
            puts("pat neg bad")
        }
    }
    p i32 = 4
    while p -= 1 {
        is 3 {
            puts("pat int ok")
            break
        }
        is _ {
            puts("pat int bad")
        }
    }
    k i32 = 5
    while k -= 1 {
        is 2..4 {
            puts("pat range ok")
            break
        }
        is _ {
            puts("pat range bad")
        }
    }
    want i32 = 4
    m i32 = 5
    while m -= 1 {
        is want {
            puts("pat ident ok")
            break
        }
        is _ {
            puts("pat ident bad")
        }
    }
    n i32 = 6
    while n -= 1 {
        is GREEN {
            puts("pat enum ok")
            break
        }
        is _ {
            puts("pat enum bad")
        }
    }
    return
}
