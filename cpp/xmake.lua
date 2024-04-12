add_rules("mode.debug", "mode.release")
add_requires("zlib", "libcurl", "spdlog", "vcpkg::cpprestsdk", "openssl")
if is_plat("windows") then
    add_requires("libiconv")
end

target("stream")
    set_kind("binary")
    set_languages("cxx20")
    add_files("src/stream.cpp")
    add_packages("libcurl", "zlib", "spdlog", "vcpkg::cpprestsdk", "openssl")
    if is_plat("linux") then
        add_linkdirs("/usr/lib/x86_64-linux-gnu")
    end
    
    if is_plat("windows") then
        add_packages("libiconv")
        add_links("bcrypt", "winhttp")
        add_defines("GBK")
    end