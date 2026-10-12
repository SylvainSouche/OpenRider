# Workspace-wide compile rules, seen by every framework and module.
# C++20, restrained: no exceptions, no RTTI.
# @impl 0f87-6acd-23b0-5b0a
CXXSTD = c++20
CXXFLAGS += -fno-exceptions -fno-rtti
