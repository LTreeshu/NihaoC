module main
use stdio
use stdlib

func main() {
    /* §12.1 flow -> flow 所有权转移：源在转移语句内仍须可读，语句结束后失效 */
    flow ptr void = malloc(i32)
    ptr.(i32) = 300
    flow new_owner void = ptr
    print("%d\n", new_owner.(i32))

    /* 赋值形态的转移：接收方接管堆对象，源随即失效 */
    n i32 = 5
    flow dst void = &n
    flow src void = malloc(i32)
    src.(i32) = 77
    dst = src
    print("%d\n", dst.(i32))
    print("%d\n", n)

    puts("flow move ok")
}
