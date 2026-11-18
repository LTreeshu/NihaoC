module main
use stdio
link "libc.so" libc as c

// NihaoC syntax demo: covers keywords, types, operators, attributes
alias Byte = u8

[[inline]]
func add(a i32, b i32) i32 {
    return a + b
}

Person struct {
    name char[32]      // string member
    age  u8
    flag u8:1          // bitfield
}

Color enum { RED, GREEN, BLUE }

func classify(x i32) i32 {
    if x > 0 {
        return 1
    } else if x < 0 {
        return -1
    } else {
        return 0
    }
}

const MAX i32 = 1024
const PI f64 = 3.14159
const FLAG u64 = 0xFF00

func main() {
    p Person
    p.age = 25
    color Color = GREEN
    sum i32 = 0

    // pointer dereference .(T) and safe deref ?.(T)
    flow ptr void = malloc(i32)
    ptr.(i32) = 42
    value i32 = ptr?.(i32)

    // slice range a..b
    arr i32[5] = {1, 2, 3, 4, 5}
    slice = arr[1..3]

    // visibility enum values
    if visof(ptr) == _flow {
        puts("flow")
    }

    // loops with pattern matching
    while v += 1 {
        is 3 {
            break
        }
        is 0..2 {
            continue
        }
        break
    }

    for i = 0; i < 10; i++ {
        sum = sum + i
    }

    // compile-time block
    cooking {
        x i32 = sizeof(i64)      // 8
        y i32 = alignof(Person)
        z i32 = offsetof(Person, age)
    }

    puts("done")
    return
}
