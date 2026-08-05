module main
func main() {
    flow p void = malloc(i32)
    p.(i32) = 42
    print(p.(i32))
    {
        flow q void = malloc(i32)
        q.(i32) = 1
    }
    puts("flow ok")
}
