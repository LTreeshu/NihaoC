module main
use stdio

Person struct { name char[] age u8 }

func main() {
    var boy Person = {"x", 13}
    var np void = &boy.name
    if structof(Person, missing, np) == &boy {
        puts("bad")
    }
}
