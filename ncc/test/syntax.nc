module main
use stdio
link "libc.so" libc

alias Byte = u8

Person struct {
    name char[]
    age u8
    flag u8:1
}

func add(a i32, b i32) i32 {
    return a + b
}
const MAX i32 = 1024

[[inline]] 
func square(x i32) i32 {
    return x * x
}


func main() {
    flow ptr void = malloc(i32)
    ptr.(i32) = 42
    value = ptr?.(i32)
    arr i32[5] = {1, 2, 3, 4, 5}
    slice = arr[1..3]
    if visof(ptr) == _flow {
        puts("flow")
    } else if sizeof(i64) == 8 {
        puts("ok")
    } else {
        puts("bad")
    }
    while i < 10 {
        i++
    }
    for i = 0; i < 10; i++ {
        continue
    }
    do value > 0 {
        value--
        break
    }
}
