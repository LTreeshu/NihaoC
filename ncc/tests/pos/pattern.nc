module main
func main() {
    value i32 = 3
    do value > 0 {
        value = value - 1
    }
    v i32 = 0
    while v += 1 {
        is 3 {
            break
        }
        break
    }
    /* 通配符模式：is _ 恒匹配循环条件值 */
    w i32 = 9
    while w -= 1 {
        is _ {
            puts("wild ok")
            break
        }
    }
    puts("pattern ok")
}
