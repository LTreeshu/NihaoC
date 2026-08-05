-- NihaoC 编译器 - xmake 构建配置
-- 使用 tcc 工具链构建（与代码生成后端保持一致，跨平台）
set_project("nihao")
set_version("0.1.0")

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
        for p in (os.getenv("PATH") or ""):gmatch("[^;:]+") do
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
            for p in (os.getenv("PATH") or ""):gmatch("[^;:]+") do
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

if is_host("windows") then
    set_toolchains("tcc")
end

target("ncc")
    set_kind("binary")
    set_targetdir("$(builddir)")
    add_files("ncc.c", "lexer.c", "parser.c", "codegen.c", "linker.c",
              "module.c", "stdlib.c", "sym.c", "type.c", "vis.c", "cgen.c",
              "native.c", "ir.c", "irparse.c", "ir_to_c.c", "ir_to_native.c")
    add_includedirs(".", path.join(tcc_dir, "libtcc"))
    if is_host("windows") and os.isfile(path.join(tcc_dir, "libtcc.dll")) then
        -- tcc 链接器不认 -l 与 GNU 导入库，直接链接 DLL 文件
        add_ldflags(path.join(tcc_dir, "libtcc.dll"), {force = true})
    elseif not is_host("windows") then
        add_links("tcc")
        add_linkdirs(path.join(tcc_dir, "lib"))
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
    target("test_" .. name)
        set_kind("binary")
        set_targetdir("$(builddir)/tests")
        add_files(src)
        add_rules("nihao")
        add_deps("ncc")
        -- IR 专属用例（子集语法，如 ir_demo/ir_ptr）仅在 IR 后端下编译，
        -- 默认后端无法构建，设为非默认 target（xmake build 不会自动构建）
        if name == "ir_ptr" or name == "ir_demo" or name == "ir_prefix" or name == "ir_array" then
            set_default(false)
        end
    target_end()
end

-- IR 子集白名单（与 tests 用例同步维护）
-- IR_SUBSET: IR 双后端（ir-c/ir-native）可编译运行的通用用例（全量后端也可跑）
-- IR_ONLY  : IR 专属用例——子集语法（如无类型指针声明），全量 parser 无法编译
local IR_SUBSET = {hello = true, ir_demo = true, ir_expr = true}
local IR_ONLY = {ir_ptr = true, ir_prefix = true, ir_array = true}

task("test")
    on_run(function ()
        import("core.project.task")
        import("core.base.option")
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
                os.exec(line .. ' || true')
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
                    cprint("${green}[PASS] pos/%s.nc", stem)
                    passed = passed + 1
                end
                ::continue_pos::
            end

            -- err: 必须编译失败 + 错误信息含期望片段
            for _, src in ipairs(os.files("tests/err/*.nc")) do
                local stem = path.basename(src)
                if filt ~= "" and not (stem .. ".nc"):find(filt, 1, true) then
                    goto continue_err
                end
                if is_ir then
                    -- IR 前端暂未实现 M2 静态检查，错误用例对其无意义
                    skipped = skipped + 1
                    cprint("  [SKIP] err/%s.nc (IR 前端暂无静态检查)", stem)
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
