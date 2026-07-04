add_rules("mode.debug", "mode.release")

-- set_toolchains("gcc")
-- 检测 tcc 是否可用
if is_plat("linux") then
    -- 方法1：直接设置 tcc 工具链
    set_toolchains("tcc")

    -- 方法2：使用 xmake 包管理（自动下载）
    -- add_requires("tinycc")
    -- set_toolchains("@tinycc")
end

-- 自定义工具链配置（高级用法）
toolchain("tcc")
    set_kind("standalone")
    set_toolset("cc", "tcc")      -- C 编译器
    set_toolset("ld", "tcc")      -- 链接器
    set_toolset("ar", "ar")       -- 静态库归档

    -- 检查 tcc 是否可用
    on_check(function (toolchain)
        return import("lib.detect.find_tool")("tcc")
    end)

    -- 加载时的额外配置
    on_load(function (toolchain)
        -- 添加 tcc 特定的宏定义
        toolchain:add("cxflags", "-DTCC_COMPILER")
        -- tcc 不支持某些高级优化，禁用
        toolchain:add("cxflags", "-fno-common")
    end)
toolchain_end()

target("token")
    set_kind("static")
    add_files("lexer.c")

target("nihao")
    set_kind("binary")
    add_files("ncc.c")
    add_deps("token")

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

