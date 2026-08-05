module main
flow create() void {
    flow p void = malloc(i32)
    p.(i32) = 99
    return p
}
func main() {
    flow x void = create()
    print(x.(i32))
    puts("transfer ok")
}
