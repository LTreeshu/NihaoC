module main

func main() {
    /* 动态数组声明：6... 初始容量 6 槽（增长留 TODO） */
    arr i32[6...] = {10, 20, 30, 40, 50, 60}
    if arr[2] == 30 && arr[5] == 60 {
        puts("dynarr ok")
    } else {
        puts("dynarr bad")
    }
    /* 纯动态数组 [...]：默认容量 8 槽 */
    buf i32[...] = {1, 2, 3}
    if buf[0] == 1 && buf[2] == 3 {
        puts("dynarr2 ok")
    } else {
        puts("dynarr2 bad")
    }

    /* 切片：arr[lo..hi] → 返回 &arr[lo]（指针） */
    p void = arr[1..4]
    v i32 = *p
    if v == 20 {
        puts("slice ok")
    } else {
        puts("slice bad")
    }
    /* 省略起始切片 arr[..2] → &arr[0] */
    p2 void = arr[..2]
    v2 i32 = *p2
    if v2 == 10 {
        puts("slice2 ok")
    } else {
        puts("slice2 bad")
    }
    return
}
