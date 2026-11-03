#!/usr/bin/env atf-sh
# The application itself, unattended: openrider --frames N opens its window
# (OpenGL 3.2 core), loads fonts and plugins, draws N frames, then quits as
# when the window is closed. Needs a desktop session: skipped without one.

# Why the GUI cannot run here, or nothing.
no_gui() {
    command -v openrider >/dev/null 2>&1 || { echo "no openrider on PATH"; return; }
    case $(uname) in
    Darwin) ;;
    CYGWIN* | MINGW* | MSYS*)
        # The Cygwin runtime is a test target: its openrider is not the
        # application users get (that is mingw-w64 or MSVC).
        if LC_ALL=C objdump -p "$(command -v openrider)" | grep -q 'DLL Name: cygwin1.dll'; then
            echo "openrider is a Cygwin build: no GUI on that runtime"
        fi ;;
    *) [ -n "${DISPLAY}${WAYLAND_DISPLAY}" ] || echo "no display (DISPLAY, WAYLAND_DISPLAY)" ;;
    esac
}

atf_test_case starts_draws_and_quits
starts_draws_and_quits_head() {
    atf_set descr "openrider opens its window, draws 120 frames and quits with status 0"
}
starts_draws_and_quits_body() {
    why=$(no_gui)
    [ -z "$why" ] || atf_skip "$why"
    # The user's own OpenRider data, settings, logs and caches stay out of it
    # (project-model obs-gui-runs-on-user-screen): every user directory is
    # this test's work directory.
    mkdir -p home appdata localappdata
    export HOME="$PWD/home" XDG_CONFIG_HOME="$PWD/home/.config" XDG_DATA_HOME="$PWD/home/.local/share" \
        XDG_CACHE_HOME="$PWD/home/.cache"
    if command -v cygpath >/dev/null 2>&1; then
        export APPDATA="$(cygpath -w "$PWD/appdata")" LOCALAPPDATA="$(cygpath -w "$PWD/localappdata")"
    fi
    # A watchdog: a window that never draws must fail the test, not hang it.
    # Only the process started here is ever stopped.
    openrider --frames 120 >out 2>err &
    pid=$!
    ( sleep 60; kill $pid 2>/dev/null ) &
    watchdog=$!
    wait $pid
    status=$?
    kill $watchdog 2>/dev/null
    [ $status -eq 0 ] || atf_fail "exit status $status: $(cat err out)"
    grep -q '^openrider: 120 frames drawn, OpenGL ' out ||
        atf_fail "no frame summary: $(cat out err)"
    cat out
}

atf_test_case usage_error
usage_error_head() {
    atf_set descr "an unknown option is refused with status 2, before any window opens"
}
usage_error_body() {
    command -v openrider >/dev/null 2>&1 || atf_skip "no openrider on PATH"
    atf_check -s exit:2 -e match:usage openrider --nope
}

atf_init_test_cases() {
    atf_add_test_case starts_draws_and_quits
    atf_add_test_case usage_error
}
