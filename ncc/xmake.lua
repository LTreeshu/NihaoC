-- NihaoC 编译器 - xmake 构建配置
-- 使用 tcc 工具链构建（与代码生成后端保持一致，跨平台）
set_project("nihao")
-- 编译器版本单一真源：发布时只改这一处，set_version 与注入 C 的 NIHAO_VERSION 都由它派生
local nihao_version = "1.0.0"
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
    add_files("ncc.c", "lexer.c", "parser.c", "linker.c",
              "module.c", "stdlib.c", "sym.c", "type.c", "vis.c", "cgen.c",
              "native.c", "ir.c", "irparse.c", "ir_to_c.c",
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
        -- IR 专属用例（子集语法，如 ir_demo/ir_ptr）仅在 IR 后端下编译，
        -- 默认后端无法构建，设为非默认 target（xmake build 不会自动构建）
        if name == "ir_ptr" or name == "ir_ptr2" or name == "ir_demo" or name == "ir_prefix" or name == "ir_array" or name == "ir_switch" or name == "ir_struct" or name == "ir_vis" or name == "ir_builtin" or name == "ir_multi" or name == "ir_fptr" or name == "ir_mr" or name == "ir_cook" or name == "ir_float" or name == "ir_slice" or name == "ir_fcall" or name == "ir_narrow" or name == "ir_narray" or name == "ir_conv" or name == "ir_str" or name == "ir_sparam" or name == "ir_bitfield" or name == "ir_arrow" or name == "ir_dynarr" or name == "ir_trunc" or name == "struct_param_prefix" then
            set_default(false)
        end
    target_end()
    ::continue_target::
end

-- IR 子集白名单（与 tests 用例同步维护）
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
local IR_SUBSET = {hello = true, ir_demo = true, ir_expr = true, ir_loop = true, p0_case = true, ir_fptr = true, p0_link = true, ir_array = true, ir_narray = true, ir_struct = true, ir_vis = true, ir_switch = true, ir_narrow = true, ir_conv = true, ir_str = true, ir_float = true, ir_fcall = true, ir_multi = true, ir_prefix = true, ir_bitfield = true, ir_ptr = true, ir_ptr2 = true, ir_goto = true, ir_nested = true, ir_is = true, ir_arrow = true, ir_dynarr = true, ir_trunc = true, param_prefix = true, return_flow = true, static_param_share = true, ir_cook = true, cooking_ct_func = true, bracket_attrs = true, is_vis_patterns = true, pattern = true, array_capacity_1d = true, struct_param_prefix = true, ir_mr = true, goto_forms = true, fx_types = true, malloc_demo = true, flow = true, flow_no_free = true, borrow = true, flow_move = true, features = true, for_init_forms = true, transfer = true, mathmod = true, module_top_forms = true, len_builtin = true, array_capacity_tiers = true}
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
local IR_ERR_COVERED = {safe_assign_removed = true, safe_dot_removed = true, linkas_not_implemented = true, bracket_attr_unknown = true, is_bad_pattern = true, is_outside_while = true, is_in_do_body = true, m2a_flow_static = true, m2b_const_flow = true, m2c_frozen = true, m2d_invalid = true, m2e_param = true, m2f_retflow = true, m2f_retflow_const_flow = true, m2f_retflow_const_static = true, m2f_retflow_const_var = true, m2f_retflow_static_flow = true, m2f_retflow_static_var = true, str_array_bad_elem = true, str_array_overflow = true, multi_arr_nostr = true, multi_arr_overflow = true}

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

        local backends = {"c", "native", "ir-c", "ir-native"}
        local be = option.get("backend") or "c"
        local all_flag = option.get("all")
        local all = (all_flag == true or all_flag == "y")
        local filt = option.get("filter") or ""
        local list = all and backends or {be}
        for _, b in ipairs(list) do
            if b ~= "c" and b ~= "native" and b ~= "ir-c" and b ~= "ir-native" then
                cprint("${red}unknown backend '%s' (c|native|ir-c|ir-native)", b)
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
                if is_ir and not IR_SUBSET[stem] and not IR_ONLY[stem] then
                    skipped = skipped + 1
                    cprint("  [SKIP] pos/%s.nc (IR 子集未覆盖)", stem)
                    goto continue_pos
                end
                if not is_ir and IR_ONLY[stem] then
                    skipped = skipped + 1
                    cprint("  [SKIP] pos/%s.nc (IR 专属用例，全量 parser 不支持子集语法)", stem)
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

                if b == "c" and strict_cc then
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
                if is_ir and not IR_ERR_COVERED[stem] then
                    -- 非 M2 错误用例：IR 前端尚未实现对应静态检查，跳过
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
