module main
use stdio
Pt struct {
    x i32
}
func main() {
    x i32 = 1
    a void = &x
    q void = a
    q->x = 2
    puts("bad")
}
