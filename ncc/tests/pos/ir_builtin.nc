module main

Person struct { name char[] age u8 score i32 }

func main() {
    /* sizeof 基本类型 */
    if sizeof(i32) == 4 {
        puts("sizeof i32 ok")
    } else {
        puts("sizeof i32 bad")
    }
    if sizeof(i64) == 8 {
        puts("sizeof i64 ok")
    } else {
        puts("sizeof i64 bad")
    }
    /* sizeof 聚合类型（8 字节槽模型：3 成员 * 8） */
    if sizeof(Person) == 24 {
        puts("sizeof struct ok")
    } else {
        puts("sizeof struct bad")
    }
    /* sizeof 数组 */
    if sizeof(i32[4]) == 16 {
        puts("array sizeof ok")
    } else {
        puts("array sizeof bad")
    }

    /* offsetof（IR 槽模型：成员序*8） */
    if offsetof(Person, age) == 8 {
        puts("offsetof ok")
    } else {
        puts("offsetof bad")
    }

    /* typeof 映射为 sizeof */
    if typeof(i32) == 4 {
        puts("typeof ok")
    } else {
        puts("typeof bad")
    }

    /* malloc：动态分配 + 指针读写（NihaoC 指针用 void 类型声明） */
    flow p void = malloc(i32)
    *p = 42
    if *p == 42 {
        puts("malloc ok")
    } else {
        puts("malloc bad")
    }
    *p = 50
    if *p == 50 {
        puts("malloc write ok")
    } else {
        puts("malloc write bad")
    }
    return
}
