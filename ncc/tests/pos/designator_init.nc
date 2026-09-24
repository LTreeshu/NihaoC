module main
use stdio

/* BNF §5 <aggregate-init> 的指定初始化形态 `<designator> ::= "." <id> "=" <expr>`
   与 §3.1 成员默认值口径：被点名的成员用给定值，未点名的取默认值 */

Cfg struct {
    port i32 = 80
    host i32 = 1
    ttl i32 = 64
}

Pt struct {
    x i32
    y i32
}

func main() {
    /* 乱序 + 省略：host 取默认 1，ttl 取默认 64 */
    a Cfg = { .port = 8080 }
    /* 全指定且乱序 */
    b Cfg = { .ttl = 7, .host = 9, .port = 2 }
    /* 指定式与成员默认值同时生效 */
    c Cfg = { .host = 5 }

    if a.port == 8080 && a.host == 1 && a.ttl == 64 {
        puts("desig default ok")
    } else {
        puts("desig default bad")
    }
    if b.port == 2 && b.host == 9 && b.ttl == 7 {
        puts("desig reorder ok")
    } else {
        puts("desig reorder bad")
    }
    if c.port == 80 && c.host == 5 && c.ttl == 64 {
        puts("desig partial ok")
    } else {
        puts("desig partial bad")
    }

    /* 无默认值的成员：指定式仍按成员名落位 */
    p Pt = { .y = 9, .x = 1 }
    if p.x == 1 && p.y == 9 { puts("desig plain ok") } else { puts("desig plain bad") }

    /* 位置式初值不走指定式分支 */
    q Pt = { 3, 4 }
    if q.x == 3 && q.y == 4 { puts("positional ok") } else { puts("positional bad") }
    return
}
