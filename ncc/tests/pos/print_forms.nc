module main
use stdio
use stdlib

/* print 的两种形态（§2.3）：
 *   print(expr)          -> 输出整型值并换行
 *   print("fmt", args..) -> C printf 直通（不自动换行）
 * puts 为 C puts 直通：只接受字符串指针，自动换行。 */
func inspect(const val void) {
    print("Inspecting value: ")
    print(val.(i32))
}

func main() {
    const s char[] = "xiaoming"
    print("[LOG] %s\n", s)

    flow p void = malloc(i32)
    p.(i32) = 200
    inspect(p)
    print(p.(i32))
}
