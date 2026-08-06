module main

func add2(a i32, b i32) i32 {
    return a + b
}

func main() {
    /* 函数指针：声明 + 赋值函数名 + 间接调用 */
    fp void(i32, i32) i32 = add2
    r i32 = fp(3, 4)
    if r == 7 {
        puts("fptr ok")
    } else {
        puts("fptr bad")
    }

    /* 重新赋值后再次间接调用 */
    fp2 void(i32, i32) i32 = add2
    if fp2(10, 20) == 30 {
        puts("fptr2 ok")
    } else {
        puts("fptr2 bad")
    }
    return
}
