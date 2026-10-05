module main
use stdio

/* BNF §3 <array-size> 容量写法的单维子集：[N] / [N..] [N...] / [..] [...] / [..N] [...N]
   省略容量取默认容量 8；len() 取声明容量。
   与 pos/array_capacity_tiers.nc 同一矩阵，只少了多维那一行——IR 前端在多维数组声明处即拒绝，
   而七档单维在 A 后端与 IR 前端输出逐行相同，故本用例入 IR_SUBSET，让容量档也受 IR 两档门禁。 */

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
    return
}
