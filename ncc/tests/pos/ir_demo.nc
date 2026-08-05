module main
use stdio

func main() {
    x i32 = 10
    y i32 = x + 5
    if y > 12 {
        puts("big")
    } else {
        puts("small")
    }
    while x < 20 {
        x = x + 1
    }
    puts("done")
    return
}
