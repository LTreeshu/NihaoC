module main
use stdio

/* BNF §3 <array-size> 四档容量写法：[N] / [N..] [N...] / [..] [...] / [..N] [...N]
   省略容量取默认容量 8；len() 取声明容量 */

func main() {
    fixed i32[4] = { 1, 2, 3, 4 }
    dyn1 i32[3..] = { 5, 6, 7 }
    dyn2 i32[2...] = { 8, 9 }
    cap1 i32[..] = { 1 }
    cap2 i32[...] = { 2, 3 }
    old1 i32[..4] = { 4, 5, 6, 7 }
    old2 i32[...2] = { 8, 9 }

    if len(fixed) != 4 { puts("tier fixed bad") } else { puts("tier fixed ok") }
    if len(dyn1) != 3 || dyn1[2] != 7 { puts("tier N.. bad") } else { puts("tier N.. ok") }
    if len(dyn2) != 2 || dyn2[1] != 9 { puts("tier N... bad") } else { puts("tier N... ok") }
    if len(cap1) != 8 || cap1[0] != 1 { puts("tier default8 bad") } else { puts("tier default8 ok") }
    if len(cap2) != 8 || cap2[1] != 3 { puts("tier default8b bad") } else { puts("tier default8b ok") }
    if len(old1) != 4 || old1[3] != 7 { puts("tier ..N bad") } else { puts("tier ..N ok") }
    if len(old2) != 2 || old2[0] != 8 { puts("tier ...N bad") } else { puts("tier ...N ok") }

    /* 多维：各维容量后缀独立生效，len() 取各维乘积 */
    m i32[2][3] = { {1,2,3}, {4,5,6} }
    if len(m) == 6 && m[1][2] == 6 { puts("tier multi ok") } else { puts("tier multi bad") }
    return
}
