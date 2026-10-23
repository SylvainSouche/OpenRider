# NetBSD: X11 (OpenGL) is under /usr/X11R7, which the compiler does not
# search, and GLFW's header includes <GL/gl.h> (imports stage only the
# headers they name). -idirafter: searched after every other directory, so
# the headers the imports staged come first. Not /usr/pkg/include: a
# directory named twice keeps its first place, and pkgsrc packages (atf)
# add it with -I, ahead of the base system's copies.
CFLAGS += -idirafter /usr/X11R7/include
CXXFLAGS += -idirafter /usr/X11R7/include

# Threads are in libpthread, not libc: libcore and its users need -pthread.
CFLAGS += -pthread
CXXFLAGS += -pthread
LDFLAGS += -pthread

# The run-time loader searches neither /usr/pkg/lib nor /usr/X11R7/lib.
LDFLAGS += -Wl,-rpath,/usr/pkg/lib -Wl,-rpath,/usr/X11R7/lib
