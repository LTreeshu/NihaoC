module main;
use stdio;

/* BNF §1.4 / §6 <empty-stmt> ::= ";" | "#"
   终止符在语句级与顶层声明级都可用，且可单独成句（空语句） */

Cfg struct {
    a i32
    b i32
};

func pair(x i32) i32 {
    return x + 1
}#

func main() {
    p Cfg;
    p.a = 1;
    p.b = 2 #
    ;
    #
    s i32 = 0
    s += p.a + p.b
    if s == 3 {
        puts("terms ok")
    } else {
        puts("terms bad")
    }
    r i32 = pair(4) #
    if r == 5 { puts("terms func ok") } else { puts("terms func bad") }
    return;
}
