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
LIB_SHARED=NO
WARN=none
