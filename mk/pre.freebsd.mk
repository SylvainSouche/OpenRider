# @impl 0f87-6ad7-b72d-32b5
# FreeBSD: ports install under /usr/local, which the compiler does not
# search, and GLFW's header includes <GL/gl.h> from there (imports stage only
# the headers they name). -idirafter: searched after every other directory,
# so the headers the imports staged always come first.
CFLAGS += -idirafter /usr/local/include
CXXFLAGS += -idirafter /usr/local/include

# Threads are in libthr, not libc: libcore and its users need -pthread.
CFLAGS += -pthread
CXXFLAGS += -pthread
LDFLAGS += -pthread
