module main
use stdio

Say struct {
    name char[9]
    say char[]
}

func add(a i32, b i32) i32 {
    return a + b
}

func main() {
    /* 切片赋值的字符串右值：按字节复制字面量（含结尾 NUL），
       闭区间右界 b 即最后一个可写字节（§5.1.4） */
    xiaoming Say
    stptr void = &xiaoming
    stptr.(char[9])[0..8] = "xiaoming"
    if xiaoming.name[8] == 0 && xiaoming.name[0] == 120 {
        puts("str slice ok")
    } else {
        puts("str slice bad")
    }

    /* 偏移切片：只覆盖 [1..5]，首元素保留 */
    buf char[6] = {0,0,0,0,0,0}
    buf[1..5] = "abcd"
    if buf[0] == 0 && buf[1] == 97 && buf[4] == 100 && buf[5] == 0 {
        puts("str slice offset ok")
    } else {
        puts("str slice offset bad")
    }

    /* 指针类型引用 + 推断声明取成员类型（数组成员退化为指针）（§5.1.4） */
    stptr.(Say).say = "NiHao I am xiaoming!"
    talk = xiaoming.say
    puts(talk)
    name = xiaoming.name
    puts(name)

    /* 函数指针调用的推断声明：类型取返回类型（§5.1.5） */
    calc void(i32, i32) i32 = add
    result = calc(10, 20)
    if result == 30 {
        puts("fptr infer ok")
    } else {
        puts("fptr infer bad")
    }
}
