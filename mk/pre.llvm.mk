# clang only: force-include a compatibility header working around the
# MacPorts libc++ / macOS SDK <float.h> partial-include clash (FLT_MIN,
# INFINITY or NAN undefined depending on include order). See the header.
# Not for clang-cl (ABI=msvc), which takes no -include and reads its argument
# as one more source file.
.if ${ABI:U} != "msvc"
_OPENRIDER_COMPAT := ${.PARSEDIR}/include/openrider_compat.h
CFLAGS += -include ${_OPENRIDER_COMPAT}
CXXFLAGS += -include ${_OPENRIDER_COMPAT}
.endif
