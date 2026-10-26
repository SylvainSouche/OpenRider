# OpenRider GUI: a thin Dear ImGui layer over the logic libraries.
PROG=openrider
LIBS=app core maptiles implot imgui glfw
.include "mk.prog.mk"

# The views draw with OpenGL directly. On macOS GLFW's own libraries bring
# it (Cocoa); elsewhere it is libGL, which GLFW's pkg-config does not list:
# its own pkg-config ("gl"), else plain -lGL. Added last, so that the
# libraries the imports built are always found first.
.if ${TARGET} != "macos" && ${TARGET} != "win"
_OPENGL_LIBS != pkg-config --libs gl 2>/dev/null || echo -lGL
LDFLAGS += ${_OPENGL_LIBS}
.endif

# The open-file dialog (file_dialog.cpp) on Windows.
.if ${TARGET} == "win"
LDFLAGS += -lcomdlg32
.endif
