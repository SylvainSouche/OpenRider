#!/usr/bin/env atf-sh
# openrider-plugin-check accepts every in-tree plugin and refuses a library
# that exports no entry point or is not of the kind asked for.

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

# `path` as the tool sees it: a native Windows path under Cygwin.
native() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        echo "$1"
    fi
}

# The in-tree plugins, one per line (ELF systems name them lib*.so.<major>).
plugins() {
    for plugin in "$1"/*/*.dylib "$1"/*/*.so "$1"/*/*.so.* "$1"/*/*.dll; do
        [ -f "$plugin" ] && echo "$plugin"
    done
}

# A shared library every system has, exporting no openrider_plugin.
system_library() {
    case $(uname) in
    Darwin) echo /usr/lib/libSystem.B.dylib ;;
    CYGWIN* | MINGW* | MSYS*) echo "$(cygpath -m "$SYSTEMROOT")/System32/version.dll" ;;
    *) ls /lib/*/libc.so.* /lib64/libc.so.* /lib/libc.so.* /usr/lib/libc.so.* 2>/dev/null | head -n 1 ;;
    esac
}

atf_test_case in_tree_plugins_pass
in_tree_plugins_pass_head() {
    atf_set descr "every plugin in plugins/<kind>/ is accepted, kind inferred from its directory"
}
in_tree_plugins_pass_body() {
    dir=$(plugins_dir)
    [ -n "$dir" ] || atf_fail "no plugins/ directory beside a bin/ on PATH"
    found=0
    for plugin in $(plugins "$dir"); do
        found=$((found + 1))
        echo "checking $plugin"
        openrider-plugin-check "$(native "$plugin")" >out 2>&1 ||
            atf_fail "$(basename "$plugin") refused (exit $?): $(cat out)"
        grep -q '^not ok' out && atf_fail "$(basename "$plugin"): $(cat out)"
        grep -q '^1\.\.3$' out || atf_fail "$(basename "$plugin"): no TAP plan: $(cat out)"
    done
    [ "$found" -gt 0 ] || atf_fail "no plugin found in $dir"
    echo "$found plugins accepted"
}

atf_test_case no_entry_point_fails
no_entry_point_fails_head() {
    atf_set descr "a library exporting no openrider_plugin is refused, with a non-zero exit (the tool's verdict)"
}
no_entry_point_fails_body() {
    lib=$(system_library)
    [ -n "$lib" ] || atf_skip "no system library known on $(uname)"
    atf_check -s not-exit:0 -o match:'^ok 1 - .*library opens' \
        -o match:'^not ok 2 - .*exports openrider_plugin' \
        -o match:'^ok 3 - .*# SKIP' -o match:'^1\.\.3$' \
        openrider-plugin-check --kind filters "$lib"
}

atf_test_case wrong_kind_fails
wrong_kind_fails_head() {
    atf_set descr "a plugin checked as another kind is refused at the descriptor"
}
wrong_kind_fails_body() {
    dir=$(plugins_dir)
    [ -n "$dir" ] || atf_fail "no plugins/ directory beside a bin/ on PATH"
    plugin=$(plugins "$dir" | grep /filters/ | head -n 1)
    [ -n "$plugin" ] || atf_fail "no filter plugin in $dir"
    atf_check -s not-exit:0 -o match:"^not ok 3 - .*'tiles' plugin" -o match:"is a 'filters' plugin" \
        openrider-plugin-check --kind tiles "$(native "$plugin")"
}

atf_test_case usage_errors
usage_errors_head() {
    atf_set descr "no plugin, or an unknown kind, is a usage error (exit 2)"
}
usage_errors_body() {
    atf_check -s exit:2 -e match:usage openrider-plugin-check
    atf_check -s exit:2 -e match:usage openrider-plugin-check --kind nope x.so
}

atf_init_test_cases() {
    atf_add_test_case in_tree_plugins_pass
    atf_add_test_case no_entry_point_fails
    atf_add_test_case wrong_kind_fails
    atf_add_test_case usage_errors
}
