module top_forms
use sys.path
link "kernel32" Kernel32
use stdio

/* BNF §2 <top-level>：use / link 可任意交错、可带 `;` / `#` 终止符；
   <use-decl> 允许点号分隔的多段模块名；§7 <bracket-attr> 的 export 带字面量参数 */

[[export ".mydata"]] const MARKER i32 = 7

[[inline]] func twice(v i32) i32 {
    return v * 2
}

func describe() string {
    return "form ok"
}
