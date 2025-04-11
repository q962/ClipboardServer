add_rules("mode.debug", "mode.release")

-- xmake:2.9.8 bug
package("gtk4")
package_end()

add_requireconfs("*", {
    system = true
})

add_requires("gtk4 >=4.16", "gmodule-2.0 >=2.80", "libadwaita-1 >=1.6.2")
add_requires("sqlite3 >=3.46")
if is_host("windows") then
    add_requires("MagickWand >=7.1")
end
add_requires("luajit >= 2.1")

if is_host("linux") then
    add_requires("gio-unix-2.0")
end

if is_host("windows") then

    target("rc")
    do
        set_kind("object")
        add_files("src/win.rc")

        on_load(function(target)
            target:add("includedirs", target:scriptdir())
        end)
    end

    target("start")
    do
        add_packages("libuv")
        add_files("src/start.c")
        add_ldflags("-mwindows")
        add_deps("rc")
        add_defines("UNICODE")

        on_install(function(target)
            local installdir = path.absolute(target:installdir())

            os.cp(target:targetfile(), installdir)
        end)
    end
end

target("gtkclip")
do
    add_packages("gtk4", "sqlite3", "gmodule-2.0", "MagickWand", "libadwaita-1")
    if is_host("linux") then
        add_packages("gio-unix-2.0")
    end
    set_prefixname("") -- remove 'lib'
    set_kind("shared")
    add_files("src/**.c")
    if not is_host("windows") then
        remove_files("src/ditto.c")
        remove_files("src/utils/image-to.c")
    end

    if is_host("windows") then
        add_deps("rc")
    end
    remove_files("src/start.c")
    add_configfiles("src/config.h.in")
    add_includedirs("$(buildir)")
    add_defines("UNICODE")

    add_installfiles("res/(glib-2.0/**)", {
        prefixdir = "share"
    })
    add_installfiles("res/(applications/**)", {
        prefixdir = "share"
    })
    add_installfiles("res/(locale/**)", {
        prefixdir = "share"
    })
    add_installfiles("res/(icons/**)", {
        prefixdir = "share"
    })
    add_installfiles("res/(" .. (get_config("APPID") or "") .. "/**)", {
        prefixdir = "share"
    })
    add_installfiles("res/(metainfo/**)", {
        prefixdir = "share"
    })

    on_load(function(target)
        target:set("configvar", "APPID", get_config("APPID"))
        target:set('configvar', "APPIDPATH", get_config("APPIDPATH"))

        local envs = LoadEnv(target)

        if is_mode("debug") then
            target:add("defines", "DEBUG");
        end

        if os.getenv("PREFIX") then
            target:add("defines", "PROJECT_PREFIX=\"" .. os.getenv("PREFIX") .. "\"");
        end

        if is_host("windows") then
            target:add('links', 'iphlpapi', "ws2_32", "MinHook", "Gdi32")
        end

        local luajit_p = import("lib.detect.find_package")("luajit")
        if luajit_p then
            target:add("includedirs", luajit_p.includedirs)
        end

        target:add("runenvs", "PATH", target:targetdir() .. "/../bin")

        gnome.compile_resources(target, "gresources", target:scriptdir() .. "/res/gresources.xml", {"--manual-register"})

        for _, filepath in ipairs(os.files(target:scriptdir() .. "/po/*.po")) do
            local basename = path.basename(filepath)
            local out_dir = target:scriptdir() .. "/res/locale/" .. basename .. "/LC_MESSAGES/"
            local out_path = out_dir .. get_config("APPID") .. ".mo"
            os.mkdir(out_dir)
            utils.mtimedo(filepath, out_path, function()
                os.execv("msgfmt", {filepath, "-o", out_path})
            end)
        end
    end)

    after_install(function(target)

        local installdir = path.absolute(target:installdir()) .. '/'
        local installdir_share = path.join(installdir, "share") .. '/'

        gnome.compile_schemas(installdir_share .. "/glib-2.0/schemas")

        if is_subhost("msys") then
            gnome.pack_gtk4(target);
        end
    end)

    after_install(function(target)
        local installdir = path.absolute(target:installdir()) .. '/'
        local installdir_share = path.join(installdir, "share") .. '/'

        gnome.compile_schemas(installdir_share .. "/glib-2.0/schemas")
    end)
end
