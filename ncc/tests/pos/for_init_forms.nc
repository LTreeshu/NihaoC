module main
use stdio

/* BNF §6 <for-stmt>：<for-init> 可以是 <var-decl>（含类型声明与括号类型），
   <for-step> 是任意表达式（赋值、`i++`/`i--`、函数调用） */

count i32 = 0

func tick() {
    count = count + 1
}

func twice(k i32) i32 {
    return k * 2
}

func main() {
    s i32 = 0
    for i i32 = 0; i < 4; i++ {
        s = s + i
    }
    if s == 6 { puts("for decl init ok") } else { puts("for decl init bad") }

    t i32 = 0
    for j i32 = 3; j > 0; j = j - 1 {
        t = t + j
    }
    if t == 6 { puts("for assign step ok") } else { puts("for assign step bad") }

    u i32 = 0
    for k = 2; k > 0; k-- {
        u = u + k
    }
    if u == 3 { puts("for infer init ok") } else { puts("for infer init bad") }

    v i32 = 0
    for m (i32) = 0; m < 2; m = m + 1 {
        v = v + twice(m)
    }
    if v == 2 { puts("for paren init ok") } else { puts("for paren init bad") }

    count = 0
    for n i32 = 0; n < 5; tick() {
        if count >= 2 {
            break
        }
    }
    if count == 2 { puts("for call step ok") } else { puts("for call step bad") }
    return
}
