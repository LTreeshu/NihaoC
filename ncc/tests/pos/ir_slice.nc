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
    v i32 = p.()
    if v == 20 {
        puts("slice ok")
    } else {
        puts("slice bad")
    }
    /* 省略起始切片 arr[..2] → &arr[0] */
    p2 void = arr[..2]
    v2 i32 = p2.()
    if v2 == 10 {
        puts("slice2 ok")
    } else {
        puts("slice2 bad")
    }

    /* 切片赋值：arr[lo..hi] = {v0, v1, ...} 批量写回 */
    arr[1..4] = {2, 3, 4, 5}
    if arr[1] == 2 && arr[3] == 4 && arr[5] == 60 {
        puts("slice assign ok")
    } else {
        puts("slice assign bad")
    }
    /* 切片读取（写回后） */
    p3 void = arr[2..4]
    v3 i32 = p3.()
    if v3 == 3 {
        puts("slice read2 ok")
    } else {
        puts("slice read2 bad")
    }
    /* 窄元素切片赋值 */
    nb i8[5] = {1, 2, 3, 4, 5}
    nb[1..3] = {9, 8, 7}
    if nb[1] == 9 && nb[3] == 7 {
        puts("slice narrow ok")
    } else {
        puts("slice narrow bad")
    }
    /* len()：数组容量 / 动态字符串长 / 切片逻辑长度（常量边界）
     * 2026-09-01 修正：len(arr) 是容量 6（声明 i32[6...]），此前误写 5；
     * 该断言此前一直输出 "len arr bad"，因本用例无 .expect 只做跨后端一致性
     * 检查而被掩盖（两后端错得一样也算 PASS）。现已补 .expect。 */
    if len(arr) == 6 {
        puts("len arr ok")
    } else {
        puts("len arr bad")
    }
    d char[] = "hello"
    if len(d) == 5 {
        puts("len str ok")
    } else {
        puts("len str bad")
    }
    sl = arr[1..4]
    /* 2026-09-01 修正：上面 arr[1..4] = {2,3,4,5} 已把 arr[1] 从 20 覆写为 2，
     * 故 sl.() 应为 2 而非 20（原断言同样被一致性检查掩盖） */
    if len(sl) == 3 && sl.() == 2 {
        puts("len slice ok")
    } else {
        puts("len slice bad")
    }
    return
}
