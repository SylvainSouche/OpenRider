#!/usr/bin/env atf-sh
# The built plugins' run-time dependencies hold no application library
# (libcore, libapp, libmaptiles): plugins use the SDK, linked in statically.

# The plugins/ directory beside a bin/ directory on PATH (the workspace's).
plugins_dir() {
    IFS=:
    for d in $PATH; do
        if [ -d "$d/../plugins" ]; then
            echo "$d/../plugins"
            return
        fi
    done
}

# Run-time dependencies of a shared library, one per line.
dependencies() {
    case $(uname) in
    Darwin) otool -L "$1" | tail -n +2 ;;
    CYGWIN* | MINGW* | MSYS*) LC_ALL=C objdump -p "$1" | grep 'DLL Name:' ;;
    *) readelf -d "$1" | grep NEEDED ;;
    esac
}

# An application library among dependencies: libcore.so, libapp.dylib, or on
# Windows core.dll, app.dll, maptiles.dll (no lib prefix there).
APP_LIBRARY='(^|[^A-Za-z0-9_])(lib)?(core|app|maptiles)[.]'

atf_test_case no_application_library
no_application_library_head() {
    atf_set descr "no plugin links libcore, libapp or libmaptiles"
}
no_application_library_body() {
    dir=$(plugins_dir)
    [ -n "$dir" ] || atf_fail "no plugins/ directory beside a bin/ on PATH"
    found=0
    for plugin in "$dir"/*/*.dylib "$dir"/*/*.so "$dir"/*/*.so.* "$dir"/*/*.dll; do
        [ -f "$plugin" ] || continue
        found=$((found + 1))
        if dependencies "$plugin" | grep -Eq "$APP_LIBRARY"; then
            atf_fail "$(basename "$plugin") depends on an application library: $(dependencies "$plugin" | grep -E "$APP_LIBRARY" | tr -s ' \t' ' ')"
        fi
    done
    [ "$found" -gt 0 ] || atf_fail "no plugin found in $dir"
}

atf_init_test_cases() {
    atf_add_test_case no_application_library
}
