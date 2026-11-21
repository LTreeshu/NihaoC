module main
use stdio

/* SH-3：函数内 PARAM 累计超过 64 条后，后续调用必须仍拿到本次实参。
 * 修前 ir_to_c 只从函数头扫到 64 条 PARAM，之后每次调用都取第 62/63 条陈旧实参
 * （4 实参调用之后的 2 实参调用被编译成上一次调用的后两个参数，产物 C 不报错、运行期算错）。 */

func add(a i32, b i32) i32 {
    return a + b
}

func quad(a i32, b i32, c i32, d i32) i32 {
    return a + b + c + d
}

func main() {
    s i32 = 0
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    s = s + add(1, 1)
    t i32 = quad(1, 2, 3, 4)
    u i32 = add(100, 200)
    print("s=%d\n", s)
    print("t=%d\n", t)
    print("u=%d\n", u)
}
