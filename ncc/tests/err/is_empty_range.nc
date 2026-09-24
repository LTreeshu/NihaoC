module main
use stdio

/* BNF §6 <pattern>：闭区间范围 `lo..hi` 在编译期校验 lo <= hi，空区间为前端错误 */

func main() {
    n i32 = 3
    while n {
        is 5..1 {
            n = 0
        }
    }
    return
}
