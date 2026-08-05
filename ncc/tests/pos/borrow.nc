module main
func main() {
    flow m void = malloc(i32)
    m.(i32) = 500
    {
        var n void = m
        n.(i32) = 600
    }
    m.(i32) = 900
    print(m.(i32))
    puts("ok")
}
