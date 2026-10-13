# Dear ImGui (MIT), fetched from its official release tag and compiled here:
# core + GLFW/OpenGL3 backends. Nothing of it is committed; the archive is
# checked against distinfo. LIBS=glfw builds libglfw.m (which stages GLFW's
# headers, needed by the GLFW backend) first.
LIB=imgui
IMPORT=fetch:imgui
FETCH_URL=https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
SRCS=imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_demo.cpp \
     backends/imgui_impl_glfw.cpp backends/imgui_impl_opengl3.cpp
IMPORT_HEADERS=imgui.h imconfig.h imgui_internal.h \
     imstb_rectpack.h imstb_textedit.h imstb_truetype.h \
     backends/imgui_impl_glfw.h backends/imgui_impl_opengl3.h \
     backends/imgui_impl_opengl3_loader.h
LIB_SHARED=NO
WARN=none
LIBS=glfw
.include "mk.lib.mk"
