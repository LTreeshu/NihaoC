module main

func main() {
    arr i32[3] = {1, 2, 3}
    x i32 = arr[0]
    y i32 = arr[2]
    arr[1] = 99
    z i32 = arr[1]
    if x == 1 {
        puts("e0 ok")
    } else {
        puts("e0 bad")
    }
    if z == 99 {
        puts("w ok")
    } else {
        puts("w bad")
    }
    i i32 = 0
    sum i32 = 0
    while i < 3 {
        sum += arr[i]
        i++
    }
    if sum == 103 {
        puts("sum ok")
    } else {
        puts("sum bad")
    }
    return
}
