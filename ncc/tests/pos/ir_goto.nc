module main
func main() {
    i i32 = 0
loop:
    i = i + 1
    if i < 3 {
        goto loop
    }
    if i == 3 {
        puts("goto ok")
    } else {
        puts("goto bad")
    }
    return
}
