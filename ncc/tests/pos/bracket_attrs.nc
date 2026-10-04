module main
use stdio

[[local]]
func helper(x i32) i32 {
    return x + 1
}

[[inline, used]] func twice(v i32) i32 {
    return v * 2
}

[[export ".mytext"]] func tagged() i32 {
    return 7
}

func main() {
    if (helper(41) == 42) {
        puts("local ok")
    }
    if (twice(3) == 6) {
        puts("inline used ok")
    }
    if (tagged() == 7) {
        puts("export ok")
    }
}
