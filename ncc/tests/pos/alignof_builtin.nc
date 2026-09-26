module main
use stdio

Pack struct {
    flag u8
    width i32
}

Pair struct {
    head u8
    body Pack
}

Mix union {
    b u8
    w f64
}

func main() {
    /* alignof(T) 由编译器在编译期算出常量（§2.3，PA-24：tcc 无 _Alignof） */
    if alignof(u8) == 1 {
        puts("align u8 ok")
    } else {
        puts("align u8 bad")
    }
    if alignof(i32) == 4 {
        puts("align i32 ok")
    } else {
        puts("align i32 bad")
    }
    if alignof(f64) == 8 {
        puts("align f64 ok")
    } else {
        puts("align f64 bad")
    }
    /* void 是通用指针类型（§5.1），对齐即指针宽度 */
    if alignof(void) == 8 {
        puts("align void-ptr ok")
    } else {
        puts("align void-ptr bad")
    }
    if alignof(Pack) == 4 {
        puts("align struct ok")
    } else {
        puts("align struct bad")
    }
    /* 嵌套结构体取成员最大对齐 */
    if alignof(Pair) == 4 {
        puts("align nested ok")
    } else {
        puts("align nested bad")
    }
    /* union 取最宽成员对齐 */
    if alignof(Mix) == 8 {
        puts("align union ok")
    } else {
        puts("align union bad")
    }
    /* 数组对齐 = 元素对齐 */
    if alignof(i32[4]) == 4 {
        puts("align array ok")
    } else {
        puts("align array bad")
    }
    if alignof(char[8]) == 1 {
        puts("align chararr ok")
    } else {
        puts("align chararr bad")
    }
    return
}
