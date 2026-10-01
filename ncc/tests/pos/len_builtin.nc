module main
use stdio

func main() {
    /* len(x) 三类逻辑长度（§2.3，BNF v2.6）：数组=元素个数、
       动态字符串=字面量长、切片变量=边界差 hi-lo */
    arr i32[6] = {10,20,30,40,50,60}
    if len(arr) == 6 {
        puts("len array ok")
    } else {
        puts("len array bad")
    }

    m i32[2][3] = {{1,2,3},{4,5,6}}
    if len(m) == 6 {
        puts("len multidim ok")
    } else {
        puts("len multidim bad")
    }

    d char[] = "hello"
    if len(d) == 5 {
        puts("len dynstr ok")
    } else {
        puts("len dynstr bad")
    }

    s = "abc"
    if len(s) == 3 {
        puts("len strlit ok")
    } else {
        puts("len strlit bad")
    }

    sl = arr[1..4]
    if len(sl) == 3 && sl.() == 20 {
        puts("len slice ok")
    } else {
        puts("len slice bad")
    }

    sw = arr[..5]
    if len(sw) == 5 {
        puts("len slice open ok")
    } else {
        puts("len slice open bad")
    }

    /* 切片读赋给固定数组：按声明容量复制，len 即容量 */
    cp i32[3] = arr[1..4]
    if len(cp) == 3 && cp[2] == 40 {
        puts("len sliced copy ok")
    } else {
        puts("len sliced copy bad")
    }

    /* 整变量重新赋值后逻辑长度不再静态可知，保守撤销（见 err 用例） */
    if len(arr) == 6 {
        puts("len stable ok")
    } else {
        puts("len stable bad")
    }
    return
}
