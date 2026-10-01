module main
use stdio

func main() {
    /* 多级指针：`[]` 逐层解引用，`.()` 省略类型，`.(T)` 固定读取宽度（§5.1.1） */
    v i32 = 7
    p1 void = &v
    p2 void[] = &p1
    p3 void[][] = &p2
    p1 = p2.()
    if p3[][].(i32) == 7 {
        puts("multilevel ok")
    } else {
        puts("multilevel bad")
    }

    /* 数组指针：`.(char[9])` 把通用指针固定成「指向 char[9]」之后再下标（§5.1.2） */
    arry char[9] = {1,2,3,4,5,6,7,8,9}
    arryptr void = &arry
    arryptr.(char[9])[0] = 0
    if arry[0] == 0 {
        puts("arrayptr ok")
    } else {
        puts("arrayptr bad")
    }
    /* 切片读赋给数组变量：按声明容量复制 */
    arrybuf char[8] = arryptr.(char[9])[0..7]
    if arrybuf[0] == 0 && arrybuf[7] == 8 {
        puts("slice copy ok")
    } else {
        puts("slice copy bad")
    }
    /* 数组指针数组 */
    arryptr2 void[2] = {&arry, &arrybuf}
    arryptr2[0].(char[9])[8] = 42
    arryptr2[1].(char[8])[7] = 9
    if arry[8] == 42 && arrybuf[7] == 9 {
        puts("ptr-of-array ok")
    } else {
        puts("ptr-of-array bad")
    }

    /* 指针数组：`void[3]` 由 malloc 初始化 → 退化为指针（§5.1.3） */
    dptrarry1 void[3] = malloc(void[3])
    dptrarry1[2] = &v
    dptrarry1[2].(i32) += 1
    if v == 8 {
        puts("dyn ptr array ok")
    } else {
        puts("dyn ptr array bad")
    }
    /* 指针数组的指针 */
    ptrarry void = &arryptr2
    ptrarry.(void[2])[0].(char[9])[3] = 5
    if arry[3] == 5 {
        puts("ptrarry ok")
    } else {
        puts("ptrarry bad")
    }

    /* 切片赋值：逐元素写回 */
    arr i32[6] = {10,20,30,40,50,60}
    arr[1..4] = {2,3,4}
    if arr[0] == 10 && arr[1] == 2 && arr[3] == 4 && arr[4] == 50 {
        puts("slice assign ok")
    } else {
        puts("slice assign bad")
    }

    /* 多维数组：声明按书写顺序成维 */
    m i32[2][3] = {{1,2,3},{4,5,6}}
    dp3 void[4][5] = malloc(void[4][5])
    dp3[3][4] = &v
    dp3[3][4].(i32) += 1
    if m[1][2] == 6 && v == 9 {
        puts("multi-dim ok")
    } else {
        puts("multi-dim bad")
    }
}
