module main
func classify(x i32) i32 {
    if x > 0 {
        return 1
    } else if x < 0 {
        return -1
    } else {
        return 0
    }
}
func main() {
    y i32 = classify(5)
    puts("ok")
}
