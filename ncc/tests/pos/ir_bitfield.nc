module main

/* 位域：槽内低 N 位（mbits 记录宽度；读=LOAD+AND 掩码，写=RMW） */
Packet struct {
    type u8
    len u8:4
    flag u8:4
    data i32
}

func main() {
    p Packet
    p.type = 1
    p.len = 10
    p.flag = 3
    p.data = 100
    if p.len == 10 && p.flag == 3 && p.type == 1 && p.data == 100 {
        puts("bitfield ok")
    } else {
        puts("bitfield bad")
    }
    /* 位域写回不破坏同槽其他位域（RMW） */
    p.len = 5
    if p.len == 5 && p.flag == 3 {
        puts("bitfield rmw ok")
    } else {
        puts("bitfield rmw bad")
    }
    /* 位域复合赋值（基于位域值） */
    p.len += 1
    if p.len == 6 && p.flag == 3 {
        puts("bitfield compound ok")
    } else {
        puts("bitfield compound bad")
    }
    /* 位域溢出截断（4 位：17 → 1） */
    p.flag = 17
    if p.flag == 1 {
        puts("bitfield trunc ok")
    } else {
        puts("bitfield trunc bad")
    }
    return
}
