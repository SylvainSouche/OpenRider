# System libraries: OpenGL, and GDI for GLFW's Win32 window (static GLFW
# carries no dependency info of its own).
LDFLAGS += -lopengl32 -lgdi32
