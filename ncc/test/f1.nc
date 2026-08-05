module main
Person struct {
    name char[32]
    age u8
    flag u8:1
}
func main() {
    p Person
    puts("ok")
}
