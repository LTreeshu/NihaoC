module main
use stdio

/* BNF §3 <type-name> ::= … | "(" <type-name> ")"，括号类型仍可接数组后缀；
   <int-literal> / <float-literal> 自带符号，表达式里不存在一元正号 */

Point struct {
    x i32
    y i32
}

func main() {
    v (i32) = 7
    a (i32)[3] = { 1, 2, 3 }
    if v == 7 && len(a) == 3 && a[2] == 3 {
        puts("paren type ok")
    } else {
        puts("paren type bad")
    }

    neg i32 = -5
    pos i32 = +5
    two i32 = neg + +7
    half f64 = -1.5
    if neg == -5 && pos == 5 && two == 2 && half == -1.5 {
        puts("signed literal ok")
    } else {
        puts("signed literal bad")
    }

    /* 括号类型也能写在函数参数与结构体成员位置之外：此处验证取地址后仍可解引用 */
    q = &v
    if q.() == 7 { puts("paren deref ok") } else { puts("paren deref bad") }

    pt Point
    r = &pt
    r->x = 3
    r->y = 4
    if pt.x == 3 && pt.y == 4 { puts("arrow typed ok") } else { puts("arrow typed bad") }
    return
}
