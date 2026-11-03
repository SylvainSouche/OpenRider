# System libraries: OpenGL, GDI for GLFW's Win32 window (static GLFW
# carries no dependency info of its own), and the common dialogs for
# File > Import GPX (file_dialog.cpp).
LDFLAGS += -lopengl32 -lgdi32 -lcomdlg32
