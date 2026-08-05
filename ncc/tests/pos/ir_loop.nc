module main

func main() {
    sum i32 = 0
    i i32 = 0
    while i < 10 {
        i++
        if i == 3 {
            continue
        }
        if i == 7 {
            break
        }
        sum += i
    }
    if sum == 18 {
        puts("while ok")
    } else {
        puts("while bad")
    }
    j i32 = 0
    for j = 0; j < 5; j++ {
        sum += j
    }
    if sum == 28 {
        puts("for ok")
    } else {
        puts("for bad")
    }
    k i32 = 0
    for k = 0; k < 6; k += 2 {
        sum += k
    }
    if sum == 28 + 0 + 2 + 4 {
        puts("forstep ok")
    } else {
        puts("forstep bad")
    }
    return
}
