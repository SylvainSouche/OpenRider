# Windows: no system GLFW package -- fetch the release and compile its Win32
# backend here (core, null and win32/wgl sources; GLFW 3.4 has no generated
# config header, the backend is chosen by -D_GLFW_WIN32 alone).
# Overrides the makefile's pkg-config import.
IMPORT=fetch:glfw
REQUIRES=
FETCH_URL=https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
SRCS=src/context.c src/init.c src/input.c src/monitor.c src/platform.c \
     src/vulkan.c src/window.c src/egl_context.c src/osmesa_context.c \
     src/null_init.c src/null_monitor.c src/null_window.c src/null_joystick.c \
     src/win32_module.c src/win32_time.c src/win32_thread.c src/win32_init.c \
     src/win32_joystick.c src/win32_monitor.c src/win32_window.c src/wgl_context.c
CFLAGS += -D_GLFW_WIN32
# Cygwin (the runtime the tests run with) does not define _WIN32: GLFW then
# takes its POSIX thread, timer and module code (platform.h) and polls with
# poll(); the window, monitor and WGL code stays Win32.
.if !empty(_TARGET_KEY:M*-cygwin)
SRCS := ${SRCS:Nsrc/win32_module.c:Nsrc/win32_time.c:Nsrc/win32_thread.c} \
     src/posix_module.c src/posix_time.c src/posix_thread.c src/posix_poll.c
.endif
LIB_SHARED=NO
WARN=none
