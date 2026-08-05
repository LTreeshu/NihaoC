module main
use stdio

func add(a i32, b i32) i32 {
    return a + b
}

func main() {
    x i32 = add(3, 4)
    if x > 5 {
        puts("big")
    } else {
        puts("small")
    }
    puts("hello nihao")
}
