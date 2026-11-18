-- NihaoC 编译器 - xmake 构建配置
-- 使用 tcc 工具链构建（与代码生成后端保持一致，跨平台）
set_project("nihao")
-- 编译器版本单一真源：发布时只改这一处，set_version 与注入 C 的 NIHAO_VERSION 都由它派生
-- 2026-11-12 A 线退役、仓库收敛 IR-only 单一管线 ⇒ 版本进入 0.2 线
local nihao_version = "0.2.0"
set_version(nihao_version)
add_defines("NIHAO_VERSION_TAG=" .. nihao_version)

add_rules("mode.debug", "mode.release")
set_languages("c99")

-- 探测 tcc 安装目录（用于 libtcc.h 头文件与 libtcc.dll 链接）
local tcc_dir = ""
do
    local envdir = os.getenv("NIHAO_TCC_DIR")
    if envdir and envdir ~= "" and (os.isfile(path.join(envdir, "tcc.exe"))
        or os.isfile(path.join(envdir, "tcc"))) then
        tcc_dir = envdir
    else
        -- path.splitenv 按平台分隔符切 PATH；gmatch("[^;:]+") 会把 Windows 盘符冒号误切成两段
        for _, p in ipairs(path.splitenv(os.getenv("PATH") or "")) do
            if os.isfile(path.join(p, "tcc.exe")) or os.isfile(path.join(p, "tcc")) then
                tcc_dir = p
                break
            end
        end
    end
    if tcc_dir == "" then tcc_dir = "." end
end

toolchain("tcc")
    set_kind("standalone")
    set_toolset("cc", "tcc")
    set_toolset("cxx", "tcc")
    set_toolset("ld", "tcc")
    set_toolset("ar", "tcc")
    set_toolset("sh", "tcc")
    on_load(function (toolchain)
        local bindir
        local envdir = os.getenv("NIHAO_TCC_DIR")
        if envdir and envdir ~= "" and (os.isfile(path.join(envdir, "tcc.exe"))
            or os.isfile(path.join(envdir, "tcc"))) then
            bindir = envdir
        else
            for _, p in ipairs(path.splitenv(os.getenv("PATH") or "")) do
                if os.isfile(path.join(p, "tcc.exe")) or os.isfile(path.join(p, "tcc")) then
                    bindir = p
                    break
                end
            end
        end
        if bindir then
            toolchain:add("bindir", bindir)
        end
    end)
toolchain_end()

-- 统一 tcc 工具链：与代码生成后端一致（ncc 本身也用 tcc 编译，跨平台行为一致）。
-- Windows 用 tcc.exe；Linux/macOS 用系统 tcc（PATH/NIHAO_TCC_DIR 探测到即启用）。
-- 注：gcc 严格模式会暴露代码中的 UB（如 lexer tok_str 悬垂在 gcc 下崩溃、tcc 下正常），
-- 统一 tcc 保持 1.0 行为基线；UB 根治留 2.0 质量阶段。
if is_host("windows") or os.isfile(path.join(tcc_dir, "tcc")) then
    set_toolchains("tcc")
end

target("ncc")
    set_kind("binary")
    set_targetdir("$(builddir)")
    -- 仅 IR 管线（2026-11-12：A 线 parser.c / cgen.c / native.c 及符号 / 类型 / 可见性
    -- / 模块 / 链接 / 内建运行时子系统全部物理删除，唯一前端是 irparse.c）
    add_files("ncc.c", "lexer.c", "ir.c", "irparse.c", "ir_to_c.c",
              "ir_backend.c", "ir_riscv64.c", "ir_loongarch64.c")
    add_includedirs(".", path.join(tcc_dir, "libtcc"))
    if is_host("windows") and os.isfile(path.join(tcc_dir, "libtcc.dll")) then
        -- tcc 链接器不认 -l 与 GNU 导入库，直接链接 DLL 文件。
        -- tcc 目录若含空格（如 "D:\Program Files\tcc"），tcc 会把路径按空格切分而链接失败，
        -- 故先复制 libtcc.dll 到构建目录再链接副本；顺带让 ncc.exe 运行时无需 PATH 找 DLL。
        local dllsrc = path.join(tcc_dir, "libtcc.dll")
        before_link(function (target)
            os.cp(dllsrc, path.join(target:targetdir(), "libtcc.dll"))
        end)
        add_ldflags(path.join("$(builddir)", "libtcc.dll"), {force = true})
    elseif not is_host("windows") then
        -- Linux：优先直链 libtcc.so。apt 的 libtcc.a 内部符号与 ncc 重名
        -- （expect/sym_find/sym_push 等），静态链接会 multiple definition 冲突（2026-08-31 实测）
        local lts = nil
        for _, d in ipairs({ path.join(tcc_dir, "lib"), "/usr/local/lib", "/usr/lib/x86_64-linux-gnu" }) do
            if os.isfile(path.join(d, "libtcc.so")) then
                lts = path.join(d, "libtcc.so")
                break
            end
        end
        if lts then
            add_ldflags(lts, {force = true})
        else
            add_links("tcc")
            add_linkdirs(path.join(tcc_dir, "lib"))
        end
    end
    set_warnings("all")
target_end()

rule("nihao")
    set_extensions(".nc")
    on_build(function (target)
        local ncc = target:dep("ncc"):targetfile()
        local src = target:sourcefiles()[1]
        local out = target:targetfile()
        os.mkdir(path.directory(out))
        local f = io.open(src, "r")
        local content = f and f:read("*a") or ""
        if f then f:close() end
        if content:find("func main", 1, true) or content:find("main(", 1, true) then
            os.execv(ncc, {"build", src, "-o", out})
        else
            os.execv(ncc, {"build", "-c", src, "-o", out})
        end
    end)
    on_run(function (target)
        local exe = target:targetfile()
        local src = target:sourcefiles()[1]
        local rc = os.execv(exe, {}, {})
        cprint("run %s -> %d", src, rc)
    end)
rule_end()

for _, src in ipairs(os.files("tests/pos/*.nc")) do
    local name = path.basename(src)
    if not is_host("windows") and name == "p0_link" then
        -- link 用例固定用 Windows kernel32（tcc 自带 .def），非 Windows 无此库；
        -- 不生成 target，否则 xmake 默认构建会失败（2026-08-31 WSL 实测 kernel32 not found）
        goto continue_target
    end
    target("test_" .. name)
        set_kind("binary")
        set_targetdir("$(builddir)/tests")
        add_files(src)
        add_rules("nihao")
        add_deps("ncc")
        -- IR 编不过默认后端（ir-c）的缺口用例：设为非默认 target，避免 `xmake build` / `xmake test`
        -- 的默认构建中止（门禁本身独立 loop 编译这些用例并计 SKIP）。2026-11-12 A 线删除后，
        -- 缺口 = 真实布局家族 9 份。
        if name == "ir_ptr" or name == "ir_ptr2" or name == "ir_demo" or name == "ir_prefix" or name == "ir_array" or name == "ir_switch" or name == "ir_struct" or name == "ir_vis" or name == "ir_builtin" or name == "ir_multi" or name == "ir_fptr" or name == "ir_mr" or name == "ir_cook" or name == "ir_float" or name == "ir_slice" or name == "ir_fcall" or name == "ir_narrow" or name == "ir_narray" or name == "ir_conv" or name == "ir_str" or name == "ir_sparam" or name == "ir_bitfield" or name == "ir_arrow" or name == "ir_dynarr" or name == "ir_trunc" or name == "struct_param_prefix"
           or name == "multi_arr_decl" or name == "str_array_init" or name == "type_forms"
           or name == "void_slot_infer" or name == "slice_str_infer" or name == "ownerof"
           or name == "designator_init" or name == "struct_member_default" or name == "array_member_default"
           or name == "mathmod" or name == "module_top_forms" then
            set_default(false)
        end
    target_end()
    ::continue_target::
end

-- IR 子集白名单（与 tests 用例同步维护）
-- 2026-11-12 A 线退役：下列注释块里的「A 后端 / 全量 parser / 四后端」是逐轮追加的历史记录，
-- 原样保留；自 A 线删除起只有 IR 双后端（ir-c / ir-native），名单语义简化为「IR 是否可编」。
-- IR_SUBSET: IR 双后端（ir-c/ir-native）可编译运行的通用用例（全量后端也可跑）
-- IR_ONLY  : IR 专属用例——子集语法（如无类型指针声明），全量 parser 无法编译
-- P11（2026-10-03）把 ir_cook 从 IR_ONLY 移进 IR_SUBSET：A 后端现已支持 <ct-func-def>，
-- 四后端都能编译运行；cooking_ct_func 随轮直接入 IR_SUBSET（irparse.c 的体源文本改按字符截取后才成立）
-- P12（2026-10-04）新增 bracket_attrs 入 IR_SUBSET：irparse.c 补上 <bracket-attr-list> 之后，
-- 属性表（含独占一行的写法）四后端一致。module_top_forms 仍不入——IR 前端没有顶层变量声明
-- PB-28（2026-10-05）新增 is_vis_patterns 入 IR_SUBSET：可见性枚举 / 枚举常量 / 标识符按值 /
-- 负数 / 闭区间 / 通配符兜底六档 is 模式实测四后端输出逐字相同。is_pattern_forms 与
-- is_no_fallthrough 仍不入——前者写了 IR 不接受的带符号区间 `is -9..-5`，后者依赖首匹配即止
-- （IR 无 __is_matched，重叠子句会全部执行）
-- 白名单普查（2026-10-05）新增 pattern 入 IR_SUBSET：`do` + `while v += 1` + `is 3 { break }`
-- 一份，ir-c / ir-native 输出与该用例 .expect 逐行相同，此前落 SKIP 只是从未被逐份复测过
-- 同轮新增 array_capacity_1d（本分支自写）：容量档矩阵里 IR 能解析的七档单维。
-- array_capacity_tiers（PA 结转）仍不入——它只多一行多维 `m i32[2][3]`，删掉该行后七档输出
-- 与 A 后端逐行相同；多维声明与无初值数组声明是 IR 的既有缺口，见下方差异表
-- PB-54（2026-10-27）把 ir_mr 从 IR_ONLY 移进 IR_SUBSET：它在 IR_ONLY 里却让 A 后端也能编，
-- 于是跨后端比对只拿到「ir-c vs ir-native」两个同前端落地器互比；实跑四端输出逐字节相同，
-- 移入后这一格升为真正的四后端一致（该用例无 .expect，故 PASS 计数不变）
-- PB-59（2026-11-05）新增 goto_forms（本分支自写）：BNF 声明的合法标签形态里，此前门禁只有
-- pos/ir_goto.nc 一份向后跳转。新用例把前跳、同名标签分处两函数、标签在内层块、从 case 体
-- goto 跳出 switch 四档一并覆盖，四后端输出逐字相同且有 .expect，故入 IR_SUBSET 并计入 PASS 档
-- PB-64（2026-11-10）flow / flow_no_free / borrow / flow_move 四份入 IR_SUBSET：`print` 在 IR 下降到
-- printf 之后，加上 PB-63 放行的 `.(T)`，这四份在四档输出逐字相同（`300/77/5`、`42`+`flow ok`、
-- `900`+`ok`、`literal/99/5`）。前三份带 .expect ⇒ IR 侧 PASS +3；`flow_move` 没有 ⇒ 只升比对档。
-- 同轮里 `str_array_init` 变成能编能跑但输出截断（IR 打 `x/9/A`、A 打 `xiaoming/9/ABCDEFGH`）——那是
-- 真实布局家族（待裁第 23 条），绝不入名单：入去就是当场红。
-- PB-63（2026-11-10）把 malloc_demo 从 A 专属移进 IR_SUBSET：IR 前端补上 `.(T)` 类型解引用（读站点与
-- 写站点各一处，新增 ir_scalar_code_of_token 与声明处同一条类型码链）之后，这份用例四档都能编、
-- 实跑输出逐字相同（`malloc ok` / `malloc write ok`）。它没有 .expect，所以升的是比对档不是 PASS 计数
-- （实测：4 后端一致格 32→33、2 后端格 4→3、IR 侧 SKIP 39→38，PASS 仍是 32；给它补 .expect 属 PA 结转用例资产，
-- 按规矩不自作主张）。同簇另外四份
-- （flow / flow_move / flow_no_free / borrow）此前也卡在 `.(T)`，现已过前端，改卡在 IR 不下降 `print`
-- （产物 C 里是裸 `print(...)` 调用，tcc 报 undefined symbol）——那是待裁第 7 条，不是这一簇的事了
-- PB-60（2026-11-09）新增 fx_types（本分支自写）：fx32 / fx64 按同宽整型这条 v2.11 定案此前
-- 在门禁里 0 份覆盖，而 IR 前端把 FX32 落到 i64 那一档（声明与形参两处类型码链都没有 FX 分支），
-- 于是同一份源码在 c / native 截断、在 ir-c / ir-native 不截断；本轮补齐两处类型码并逐形态实测四后端
-- 输出逐字相同（含二次赋值截断与 fx64 宽整型对照），两交叉档也出码过汇编
-- PB-66（2026-11-11）新增 features / for_init_forms / transfer / mathmod / module_top_forms 五份：
-- 这五份此前在 IR 侧挂「IR 子集未覆盖」，真因是 IR 前端没有顶层声明（顶层 var/const、模块级
-- func、alias、for 的声明式 init 与任意表达式 step、else if 链、struct 数组成员）。
-- PB-66 在 irparse.c 补齐这些形态之后，四档实跑输出逐字相同（transfer `99`+`transfer ok`、
-- for_init_forms 五行、features `done`），两份模块型用例（mathmod / module_top_forms）
-- 四档都出 .c 产物 ⇒ 入 IR_SUBSET，IR 侧 PASS 35→40、SKIP 34→29。
-- 边界照旧诚实：`char[32]` 这类数组成员在 IR 侧仍只占 1 个 8 字节槽（真实布局是待裁第 23 条），
-- features 恰好不回读该成员才逐字相同——它进名单是因为输出相同，不是因为布局已定案。
-- PB-67（2026-11-11）新增 len_builtin / array_capacity_tiers 两份：这两份此前挂在「数组 / 切片
-- 容量档声明形态」一簇，PB-67 在 irparse.c 补齐四件事之后四档实跑输出逐字相同——
-- ① 多维数组声明 `m i32[2][3]`（按行主序摊平成连续槽，len() 取各维乘积），
-- ② 切片读入声明数组 `cp i32[3] = arr[1..4]`（复制个数取目标声明容量，与 A 侧 memcpy 同口径），
-- ③ 开区间切片 rhs `sw = arr[..5]`（指针 = &arr[0]，长度 = hi），
-- ④ 字面量推断字符串的 len()（`s = "abc"` → 长度 3，此前 IR 静默给 0）。
-- 两份都带 .expect ⇒ IR 侧 PASS +2、SKIP −2。第三份 deref_slice 仍不入——它的真因不是容量档，
-- 而是「指针类型后缀档」（`void[][]` 逐层解引用 / `.(char[9])` 数组型后缀 / 指针数组），已改记台账。
-- PB-68（2026-11-12）新增 deref_slice：上一轮改记的「指针类型后缀档」本轮在 irparse.c 落地，
-- 四档实跑输出逐字相同且与 .expect 逐字节相同（八行 ok）⇒ 入 IR_SUBSET，IR 侧 PASS 42→43、SKIP 27→26。
-- 落地口径：新增后缀链求值器（ir_scan_postfix / ir_chain / ir_chain_assign），只在「根变量是 malloc
-- 退化的指针值数组」「链含新形态（空下标 p[] / 数组型后缀 .(T[N])）」「链有两组以上后缀」三种情况下接管，
-- 单组旧形态一律走原路径，故门禁其余各档计数一字未变。`void[N] = malloc(void[N])` 按标量指针槽声明
-- （A 侧同形态是 `void*(*p) = malloc(N*sizeof(void*))`），malloc 字节数改由 ir_malloc_size 算（void 计 8）。
-- 边界照旧诚实：① IR 的 `char[9]` 仍是 9 个 8 字节槽（A 侧是 9 字节紧凑布局），`.(char[9])[k]` 按槽宽 8
-- 寻址才与 IR 自己的 `arry[k]` 自洽——真实布局仍是待裁第 23 条，本份进名单是因为四档输出相同，不是因为布局已定案；
-- ② 两交叉档对本份**出码即被帧护栏拒**（main 帧 3328 字节 > 2047 寻址限），拆函数探针能出码、能链接、
-- 0 告警，但八组里有三组算错地址：IR_ADDR / IR_ELEM_ADDR 在两档发射器是**向下**生长、在 ir_to_c 是**向上**
-- （C 数组），此前 is_ptr=1 的寻址只指向堆或字符串池（向上）故未暴露，本轮新形态第一次把帧地址装进指针值
-- 再做下标运算才显形。该方向分歧带设计权重（改哪一侧要动整条交叉出码面），**登记不修**，见台账与待裁清单。
-- PB-70（2026-11-12）新增 is_pattern_forms 与 stmt_terminators：两份都带 .expect，四档实跑输出与 .expect
-- 逐字节相同 ⇒ 入 IR_SUBSET，IR 侧 PASS 43→45、SKIP 26→24。落地三处，全在 irparse.c：
-- ① `is` 模式的整数字面量可带前缀符号、闭区间两端都可带（`is +5` / `is -9..-5`），新助手 ir_is_int；
--    畸形形态仍是一态一诊断（`is +x` 只报 expected integer after '+'），三份既有 err 用例诊断逐字未变。
-- ② 语句终止符 `;` / `#`：ir_stmt 拆成包装层 + ir_stmt_impl，尾部由 ir_eat_stmt_terminator 吃一个，
--    空语句在 impl 入口处理，顶层另有空声明分支（`module main;` / `use stdio;` / `Cfg struct {…};` / `}#`）。
-- ③ `return;` / `return #` 按无值返回处理（此前把终止符当表达式解析）。
-- 边界照旧诚实：stmt_terminators 里 `Cfg struct { a i32 b i32 }` 在 IR 只占 1 个 8 字节槽，实测产物 C 里
-- `p.a = 1` 写 &p、而 `p.b = 2` 写的是**无关临时槽 &t1**——输出恰好为 3 只因 t1 在被读回前没被复用。
-- 这是待裁第 23 条（聚合体真实布局）家族的又一实例，比 PB-48 记的「落在相邻槽」更尖锐：落点是任意临时槽。
-- 本份进名单是因为四档输出相同，**不是因为布局已定案**；成员寻址代码本轮一行未改。
-- PB-70 同轮另新增 literal_forms：补齐三个零覆盖的 BNF 形态后四档输出与 .expect 逐字节相同 ⇒ PASS 45→46。
-- ④ 一元 `+` 作字面量前缀（BNF §1.3，只在紧跟数字字面量时消费，其余位置的 '+' 仍是二元运算符，
--    与 A 后端 parse_primary 同口径，不放宽成一元加）；⑤ 布尔字面量 true / false 按 1 / 0 整型常量落；
-- ⑥ 声明型 `s string = "lit"` 的逻辑长度登记（PB-67 只覆盖了字面量推断与 char[] 两条路径，
--    声明型 string 的 ve[] 一直是 0 ⇒ len(s) 静默给 0）——只登记长度，不改 vetyp/寻址宽度。
-- 括号类型 `(T)` / `(T)[N]` 也同轮落地（归一成内层类型 token 走同一条声明路径），但 type_forms **不入名单**：
-- 它四条断言里 `arrow typed` 仍打 bad——`r->y` 是聚合体第二成员、越出 IR 的 1 个 8 字节槽，属待裁第 23 条。
-- （2026-11-18 PB-76 更正：聚合体改为整块连续槽 + ir-c 单条 C 数组之后 `arrow typed` 在 ir-c/ir-native
-- 已正确；两交叉档仍是 bad（帧槽向下生长，`&p+8` 向高地址走），属待裁第 33 条——故 type_forms 暂不入名单。）
-- PB-71 / PB-72（2026-11-12）新增 is_no_fallthrough（pos）与 len_unknown / void_subscript（err）：
-- ⇒ IR 侧 PASS 46→49、SKIP 23→20。三处落地全在 irparse.c，不新增 IR 操作码，发射器与 A 线一行未改：
-- ① 首匹配即止（A 侧 __is_matched，PA-16 / P4）：新增 is_matched_vreg，每轮 while 迭代在条件检查后、
--    body 之前重置为 0；每条 is 子句开头 IR_JNZ 跳过、模式匹配后在进块前 MOV 置 1。A 侧把置位放在守卫
--    条件里（`&& (__is_matched = 1)`），IR 无短路表达式故改为条件跳转之后发一条 MOV，语义等价。
--    is_no_fallthrough 改前 IR 打 14 行（多 wild/range）、改后 7 行与 .expect 逐字节相同。
-- PB-73（2026-11-12）新增 use_mod：IR 侧模块加载落地（内联进同一个 IrProg，与 A 侧 module_import 同语义）。
-- `ir_parse_file` 的顶层声明循环抽成可重入的 ir_parse_top_level；`use <name>` 由 ir_use_decl 累积点号多段名
-- 后触发 ir_use_module——按 cwd / 源文件目录 / stdlib 三层解析模块文件（点号名按目录层级），保存/还原 lexer
-- 后递归解析进当前 IrProg，错误定位落在模块自己的文件名与行号上；循环 import 由 ir_imported 名单挡。
-- ② len() 的静态可知性：新增 vlen_known 表（A 侧 Symbol.len_known 同判据）。ve[] 的 0 既表示标量也表示
--    空动态字符串，光看 ve>0 会把后者误判 ⇒ 单开标志表，只在数组声明 / 字符串字面量初值 / 常量边界切片
--    这些真正确定长度处登记。此前 len(标量) 静默给 0 并产出可执行文件（台账 E-c 桶）。
-- ③ 通用 void 指针的裸下标 / 切片：ir_check_void_subscript，与 A 侧同判据同文案。两条例外——空下标 `p[]`
--    是省略类型解引用（合法）、PB-68 的 VM_PTRARR（ve[] 为 0 但可下标）。此前静默接受非法程序。
-- PB-76（2026-11-18）聚合体改「整块连续槽」布局后 `r->y` 落到 `p.y`
-- （此前的 1 槽/成员各自 ALLOCA 在 ir-c 里是互不相邻的 C 标量，`&p + 8` 写到栈上无关位置）。
-- 实现：新增 IR_AGG_ALLOCA（基槽一次分配 imm=8*mslots，汇编后端与 ALLOCA 一样跳过、**两档 .s 全语料逐字节不变**）；
-- ir-c 把整块落成一条 C 数组 `int64_t name[slots]`，成员槽 vreg 映射为 `name[k]`，不单独声明。
-- type_forms 本轮**仍不入名单**：ir-c / ir-native 四条断言已与 .expect 相同，但两交叉档实跑
-- `arrow typed` 仍打 bad——汇编帧槽向下生长，`&p + 8` 向高地址走、离开成员槽，正是待裁第 33 条
-- （帧生长方向）的既有分歧；本轮 asm 出码未变（旧的 AGREE 是「指针写+指针读」自洽造成的假一致）。
-- 等第 33 条裁定后连同交叉档一起转正。
-- 边界照旧诚实：成员仍各占 8 字节槽（非字节紧凑），数组成员仍按 1 槽 + 首 8 字节可寻址；
-- 字节级真实布局仍是待裁第 23 条，`str_array_init` / `slice_str_infer` 等仍不入名单。
-- PB-79（2026-11-18）新增 struct_member_default / designator_init：聚合成员默认值此前
-- 「解析后忽略」，指定式初值 `.field = v` 也解析不了。本轮把默认值落成 mdef_kind/int/dbl/str
-- 表，在「无初值声明」与「初值列表未点名成员」两处回填（union 只取首个默认成员、有用户初值则不补），
-- 指定式按成员名落位；顺带让浮点成员的读值带 double 标记。两份与 .expect 逐字节相同，
-- 两交叉档 qemu 实跑输出也一致（八行 / 五行）⇒ 入名单，IR 侧 PASS 52→54、SKIP 18→16。
-- 边界：嵌套聚合成员的默认值按递归回填；数组成员/数组的聚合默认值仍属待裁第 23 条家族。
-- PB-80（2026-11-18）新增 array_member_default：聚合体数组 `P arr[N]`（含 `[...]`、多维、扁平初值、
-- 逐元素默认值、函数体内类型定义）落地后与 .expect 逐字节相同 ⇒ 入名单，IR 侧 PASS 54→55、SKIP 16→15。
-- 边界：本份 main 的帧 3456 > 2047，两交叉档**出码即被帧护栏拒**（既有上限）；语义用缩小探针
-- （P[2] 三组：默认值 / 部分位置初值 / 指定式）在两交叉档 qemu 实跑，与 ir-c 输出逐字相同。
-- PB-81（2026-11-18）新增 type_forms / void_slot_infer：ltree 对「帧生长方向」（待裁第 33 条）选
-- (c) provenance——指针变量记「来自帧地址」位（vfrm），成员偏移对帧来源改用方向感知的 IR_ELEM_ADDR
-- （汇编减 / C 加），堆/池来源仍用字节 ADD。两交叉档 qemu 实跑从 `arrow bad` 变 `arrow ok` ⇒ 入名单，
-- IR 侧 PASS 55→57、SKIP 15→13。全语料交叉 .s 的变化面仅 6 份聚合指针用例（其余逐字节不变）。
-- PB-82（2026-11-18，第 23 条解锁后第一刀）新增 str_array_init / multi_arr_decl：定长 char/i8/u8
-- 数组改为**字节紧凑存储**（块按 ceil(N*es/8) 槽分配，元素步长进 velemsz），块内字节从最低地址向上
-- （新 IR_ADDR_LO：C=&name[0]、汇编=块最低槽），字符串字面量初值逐字节写入；裸数组名退化为块首地址，
-- 链求值的元素宽随 `.(T[N])` 后缀走。两份与 .expect 逐字节相同，两交叉档 qemu 实跑同输出
-- （`xiaoming/9/ABCDEFGH/3/ab`、`cap3 ok/aa/cc/…`）⇒ 入名单，IR 侧 PASS 57→59、SKIP 13→11。
-- 边界：聚合体成员仍按 8 字节槽（数组成员真实宽/字节偏移是下一刀，ownerof / slice_str_infer 仍缺）。
local IR_SUBSET = {hello = true, ir_demo = true, ir_expr = true, ir_loop = true, p0_case = true, ir_fptr = true, p0_link = true, ir_array = true, ir_narray = true, ir_struct = true, ir_vis = true, ir_switch = true, ir_narrow = true, ir_conv = true, ir_str = true, ir_float = true, ir_fcall = true, ir_multi = true, ir_prefix = true, ir_bitfield = true, ir_ptr = true, ir_ptr2 = true, ir_goto = true, ir_nested = true, ir_is = true, ir_arrow = true, ir_dynarr = true, ir_trunc = true, param_prefix = true, return_flow = true, static_param_share = true, ir_cook = true, cooking_ct_func = true, bracket_attrs = true, is_vis_patterns = true, pattern = true, array_capacity_1d = true, struct_param_prefix = true, ir_mr = true, goto_forms = true, fx_types = true, malloc_demo = true, flow = true, flow_no_free = true, borrow = true, flow_move = true, features = true, for_init_forms = true, transfer = true, mathmod = true, module_top_forms = true, len_builtin = true, array_capacity_tiers = true, deref_slice = true, is_pattern_forms = true, stmt_terminators = true, literal_forms = true, is_no_fallthrough = true, use_mod = true, alignof_builtin = true, struct_member_default = true, designator_init = true, array_member_default = true, type_forms = true, void_slot_infer = true, str_array_init = true, multi_arr_decl = true}
local IR_ONLY = {ir_builtin = true, ir_slice = true, ir_sparam = true}
-- IR_ERR_COVERED: err 用例中 IR 前端已实现对应诊断的（未列出的非 m2 用例仍在 IR 侧跳过）
-- cooking_bare_var 刻意不入：IR 前端对块内裸 <var-decl> 只给 `ir: undeclared variable 'K'`，
-- 与共用 .expect 的 'unsupported cooking item' 子串不符，入名单等于把 IR 缺口标成已完成
-- bracket_attr_unknown 入名单：P12 起两个前端同判据，IR 侧只是文案多一个 `ir: ` 前缀，子串照样命中
-- is_bad_pattern 入名单：畸形 is 模式两个前端逐条同位置拒绝，共用子串 `'is' pattern` 双方命中
-- PB-62（2026-11-10）新增四份字符串 / 定长数组初始化诊断：IR 前端补齐与 A 后端同判据的三条检查
-- （元素非 char 不能用字符串初始化、含结尾 NUL 必须放得下、定长数组的多变量声明只收字符串字面量初值），
-- 此前这四类非法程序在 IR 线静默通过——`str_array_overflow` 甚至照样产出可执行文件。台账 E-c 桶余 2 份
-- （len_unknown 需 len_known 表、void_subscript 需指针指向类型跟踪）
-- PB-61（2026-11-10）把 11 份 m2 族从「靠 stem:startswith("m2") 这条命名豁免才进档」改成显式列入
-- IR_ERR_COVERED，并删掉那条前缀豁免：此后「IR 侧跑哪些 err」只由名单决定。豁免的实际效果此前无人
-- 复核：实跑证明这 11 份的 IR 侧 stderr 都含完整 .expect 子串（同判据、文案同源），删除豁免不改变行为。
-- is_dangling_range 刻意不入：IR 侧同样逐条拒绝，但文案 `bad 'is' pattern (expected integer
-- after '..')` 与 A 侧 `invalid 'is' range` 没有共用子串，入名单只是把判据放宽
-- is_outside_while / is_in_do_body 入名单（白名单普查 2026-10-05）：P4 曾登记「IR 文案带 `ir: `
-- 前缀故与 A 侧不同源、不能进 IR_ERR_COVERED」，本轮复测证伪——前缀在子串**之前**，共用子串
-- `'is' pattern match only valid inside while loop body` 照样整体命中；`PA` 侧同判据（其
-- IR_ERR_SKIP 黑名单从未含这两份，即 PA 的 IR 门禁一直真跑它们）
local IR_ERR_COVERED = {safe_assign_removed = true, safe_dot_removed = true, linkas_not_implemented = true, bracket_attr_unknown = true, is_bad_pattern = true, is_outside_while = true, is_in_do_body = true, m2a_flow_static = true, m2b_const_flow = true, m2c_frozen = true, m2d_invalid = true, m2e_param = true, m2f_retflow = true, m2f_retflow_const_flow = true, m2f_retflow_const_static = true, m2f_retflow_const_var = true, m2f_retflow_static_flow = true, m2f_retflow_static_var = true, str_array_bad_elem = true, str_array_overflow = true, multi_arr_nostr = true, multi_arr_overflow = true, len_unknown = true, void_subscript = true, deref_void_type = true}

task("test")
    on_run(function ()
        import("core.project.task")
        import("core.base.option")
        import("lib.detect.find_program")
        task.run("build", {targets = "ncc"})
        local ncc = path.join(os.projectdir(), "build",
                              "ncc" .. (is_host("windows") and ".exe" or ""))

        local function try_execv(program, argv, opts)
            local parts = {}
            for _, a in ipairs({program, table.unpack(argv or {})}) do
                table.insert(parts, '"' .. a .. '"')
            end
            local line = table.concat(parts, " ")
            -- 重定向内嵌进命令串（cmd 解释），避免 xmake opt 重定向在 shell 包装下失效
            if opts then
                if opts.stdout and opts.stdout ~= os.nul then
                    line = line .. ' >"' .. opts.stdout .. '"'
                end
                if opts.stderr and opts.stderr ~= os.nul then
                    line = line .. ' 2>"' .. opts.stderr .. '"'
                end
            end
            if is_host("windows") then
                os.exec('cmd /c "' .. line .. ' || exit 0"')
            else
                -- xmake 3.1.1 的 os.exec 在 Linux 上不走 shell（命令串整体作为单个
                -- 程序直接 exec），`|| true` 会被当作参数传给程序（ncc 实测报
                -- "cannot open file 'true'"）。必须显式 /bin/sh -c 包装，与
                -- Windows 的 cmd /c 分支对称（2026-08-31 WSL 实测）
                os.exec('/bin/sh -c "' .. line:gsub('"', '\\"') .. ' || true"')
            end
        end

        local norm = function(s)
            local t = {}
            for line in (s .. "\n"):gmatch("([^\n]*)\n") do
                table.insert(t, (line:gsub("\r$", "")))
            end
            return t
        end

        local backends = {"ir-c", "ir-native"}
        local be = option.get("backend") or "ir-c"
        local all_flag = option.get("all")
        local all = (all_flag == true or all_flag == "y")
        local filt = option.get("filter") or ""
        local list = all and backends or {be}
        for _, b in ipairs(list) do
            if b ~= "ir-c" and b ~= "ir-native" then
                cprint("${red}unknown backend '%s' (ir-c|ir-native)", b)
                os.exit(1)
            end
        end

        local total_failed = 0
        -- 后端一致性防线：无 .expect 的用例，收集各后端输出，
        -- 全部后端运行完后比对一致性（避免"期望缺失直接 PASS"的测试盲区）
        local consistency = {}

        -- 可移植性抽查：c 后端的产物 C 必须能被 tcc 之外的编译器解析。tcc 把「字符串
        -- 字面量内含真换行字节」当扩展容忍（PA-50/F2 的成因），gcc / clang / MSVC 会拒。
        -- 先探测该编译器在本机可用（msys gcc 缺 include 路径时会全线误报），不可用则跳过。
        -- STRICT_SKIP：可移植性抽查（-fsyntax-only）的豁免名单。2026-11-12 删 A 线后抽查由
        -- c 档挪到 ir-c 档，暴露了 ir-c 产物「puts/printf 的字符串实参按 int64_t 槽传、无
        -- (char*) cast」的历史缺陷（PB-52 家族：ir-c 产物 C 此前从未被门禁查过）。
        -- 修它属另一条欠账（ir_to_c 的 CALL 参数 cast），先豁免不挡门禁。
        local STRICT_SKIP = {flow_no_free = true}
        local strict_cc = nil
        do
            local probe = path.join(os.projectdir(), "build", ".strict_probe.c")
            local pf = io.open(probe, "w")
            if pf then
                pf:write('#include <stdio.h>\nint main(void){puts("");return 0;}\n')
                pf:close()
                for _, cand in ipairs({"clang", "gcc", "cc"}) do
                    local p = find_program(cand)
                    if p and os.execv(p, {"-fsyntax-only", probe},
                                      {try = true, stdout = os.nul, stderr = os.nul}) == 0 then
                        strict_cc = p
                        break
                    end
                end
            end
        end
        for _, b in ipairs(list) do
            local passed, failed, skipped = 0, 0, 0
            local is_ir = (b == "ir-c" or b == "ir-native")
            cprint("${cyan}\n>>> backend: %s", b)

            -- pos: 编译 + 运行 + expect 比对
            for _, src in ipairs(os.files("tests/pos/*.nc")) do
                local stem = path.basename(src)
                if filt ~= "" and not (stem .. ".nc"):find(filt, 1, true) then
                    goto continue_pos
                end
                if not is_host("windows") and stem == "p0_link" then
                    -- link 用例固定用 Windows kernel32（tcc 自带 .def），非 Windows 无此库
                    skipped = skipped + 1
                    cprint("  [SKIP] pos/%s.nc (link kernel32 仅 Windows)", stem)
                    goto continue_pos
                end
                if not IR_SUBSET[stem] and not IR_ONLY[stem] then
                    skipped = skipped + 1
                    cprint("  [SKIP] pos/%s.nc (IR 子集未覆盖)", stem)
                    goto continue_pos
                end

                local exe = path.join(os.projectdir(), "build", "tests", b,
                                      stem .. (is_host("windows") and ".exe" or ""))
                os.mkdir(path.directory(exe))

                local f = io.open(src, "r")
                local content = f and f:read("*a") or ""
                if f then f:close() end
                local has_main = content:find("func main", 1, true) or content:find("main(", 1, true)

                local args = has_main and {"build", "-backend=" .. b, src, "-o", exe}
                                     or {"build", "-backend=" .. b, "-c", src, "-o", exe}
                try_execv(ncc, args, {stdout = os.nul, stderr = os.nul})
                local okc = has_main and os.isfile(exe) or os.isfile(exe .. ".c")
                if not okc then
                    cprint("${red}[FAIL] pos/%s.nc: compile error", stem)
                    failed = failed + 1
                    goto continue_pos
                elseif not has_main then
                    cprint("${green}[PASS] pos/%s.nc (module)", stem)
                    passed = passed + 1
                    goto continue_pos
                elseif not os.isfile(exe) then
                    cprint("${red}[FAIL] pos/%s.nc: no executable produced", stem)
                    failed = failed + 1
                    goto continue_pos
                end

                if b == "ir-c" and strict_cc and not STRICT_SKIP[stem] then
                    local csrc = exe .. ".c"
                    if os.isfile(csrc) and
                       os.execv(strict_cc, {"-fsyntax-only", csrc},
                                {try = true, stdout = os.nul, stderr = os.nul}) ~= 0 then
                        cprint("${red}[FAIL] pos/%s.nc: 产物 C 被 %s 拒绝（tcc 之外的编译器不可解析）",
                               stem, path.filename(strict_cc))
                        failed = failed + 1
                        goto continue_pos
                    end
                end

                local outfile = exe .. ".out"
                local of = io.open(outfile, "w")
                if of then of:close() end
                try_execv(exe, {}, {stdout = outfile})
                local out = ""
                local f2 = io.open(outfile, "r")
                if f2 then out = f2:read("*a"); f2:close() end
                local expect = path.join(path.directory(src), stem .. ".expect")
                if os.isfile(expect) then
                    local f3 = io.open(expect, "r")
                    local want = f3:read("*a")
                    f3:close()
                    local w, g = norm(want), norm(out)
                    local match = (#w == #g)
                    if match then
                        for i = 1, #w do
                            if w[i] ~= g[i] then match = false; break end
                        end
                    end
                    if match then
                        cprint("${green}[PASS] pos/%s.nc", stem)
                        passed = passed + 1
                    else
                        cprint("${red}[FAIL] pos/%s.nc: output mismatch\n  want: %s\n  got:  %s",
                               stem, table.concat(w, "|"), table.concat(g, "|"))
                        failed = failed + 1
                    end
                else
                    -- 无 .expect：不直接 PASS——收集输出交给后端一致性检查
                    consistency[stem] = consistency[stem] or {}
                    consistency[stem][b] = out
                end
                ::continue_pos::
            end

            -- err: 必须编译失败 + 错误信息含期望片段
            for _, src in ipairs(os.files("tests/err/*.nc")) do
                local stem = path.basename(src)
                if filt ~= "" and not (stem .. ".nc"):find(filt, 1, true) then
                    goto continue_err
                end
                if not IR_ERR_COVERED[stem] then
                    -- IR 前端尚未实现对应静态检查，跳过
                    skipped = skipped + 1
                    cprint("  [SKIP] err/%s.nc (IR 前端暂未覆盖该静态检查)", stem)
                    goto continue_err
                end

                local out = path.join(os.projectdir(), "build", "tests", "err", b, stem)
                os.mkdir(path.directory(out))
                local errfile = out .. ".err"
                local ef = io.open(errfile, "w")
                if ef then ef:close() end
                try_execv(ncc, {"build", "-backend=" .. b, src, "-o", out},
                          {stdout = os.nul, stderr = errfile})
                local okc = os.isfile(out) or os.isfile(out .. ".exe")
                if okc then
                    cprint("${red}[FAIL] err/%s.nc: expected compile error, but succeeded", stem)
                    failed = failed + 1
                else
                    local expect = path.join(path.directory(src), stem .. ".expect")
                    local msg = ""
                    if os.isfile(errfile) then
                        local f = io.open(errfile, "r")
                        msg = f:read("*a") or ""
                        f:close()
                    end
                    if os.isfile(expect) then
                        local f = io.open(expect, "r")
                        local want = f:read("*a"):gsub("%s+$", "")
                        f:close()
                        if msg:find(want, 1, true) then
                            cprint("${green}[PASS] err/%s.nc", stem)
                            passed = passed + 1
                        else
                            cprint("${red}[FAIL] err/%s.nc: message missing '%s'\n  got: %s",
                                   stem, want, msg:gsub("%s+$", ""))
                            failed = failed + 1
                        end
                    else
                        cprint("${green}[PASS] err/%s.nc", stem)
                        passed = passed + 1
                    end
                end
                ::continue_err::
            end

            cprint("${yellow}\n== [%s] %d passed, %d failed, %d skipped ==",
                   b, passed, failed, skipped)
            total_failed = total_failed + failed
        end

        -- 后端一致性检查：无 .expect 的用例必须在多个后端输出一致
        local ccount = 0
        for _k, _v in pairs(consistency or {}) do ccount = ccount + 1 end
        if ccount > 0 then
            cprint("${cyan}\n>>> cross-backend consistency")
            for stem, outs in pairs(consistency) do
                local nb = 0
                local ref = nil
                local ref_be = nil
                local bad = nil
                for _, be2 in ipairs(backends) do
                    local o = outs[be2]
                    if o ~= nil then
                        if nb == 0 then
                            ref = o; ref_be = be2
                        elseif o ~= ref then
                            bad = bad or {}
                            bad[#bad + 1] = be2
                        end
                        nb = nb + 1
                    end
                end
                if nb >= 2 and bad then
                    cprint("${red}[FAIL] pos/%s.nc: inconsistent across backends", stem)
                    for _, be2 in ipairs(bad) do
                        cprint("  %s (%s): %s", be2, ref_be,
                               table.concat(norm(outs[be2]), "|"):gsub("%s+$", ""))
                    end
                    cprint("  ref (%s): %s", ref_be,
                           table.concat(norm(ref), "|"):gsub("%s+$", ""))
                    total_failed = total_failed + 1
                elseif nb >= 2 then
                    cprint("${green}[PASS] pos/%s.nc (consistent, %d backends)", stem, nb)
                else
                    cprint("${dim}[NOTE] pos/%s.nc (single backend, no consistency check)", stem)
                end
            end
        end
        if total_failed > 0 then
            os.exit(1)
        end
    end)
    set_menu {
        usage   = "xmake test [-b c|native|ir-c|ir-native] [--all] [-f filter]",
        description = "Run the NihaoC regression suite (pos + err) across backends",
        options = {
            {'b', 'backend', 'kv', nil, 'backend to test: c (default), native, ir-c, ir-native'},
            {'a', 'all', 'k', nil, 'run the full 4-backend matrix'},
            {'f', 'filter', 'kv', nil, 'substring filter on test file names'}
        }
    }
task_end()
