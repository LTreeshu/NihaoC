module main
use stdio

alias Byte = u8

Person struct {
    name char[32]
    age u8
    flag u8:1
}

Color enum { RED, GREEN, BLUE }

func add(a i32, b i32) i32 {
    return a + b
}

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
    p Person
    p.age = 25
    p.name[0] = 'a'
    color Color = GREEN
    sum i32 = 0
    for i = 0; i < 5; i++ {
        sum = sum + i
    }
    flow ptr void = malloc(i32)
    ptr.(i32) = 42
    value i32 = ptr.(i32)
    x i32 = 3
    while x > 0 {
        x = x - 1
    }
    puts("done")
}
