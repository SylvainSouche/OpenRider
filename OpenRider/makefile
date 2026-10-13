# OpenRider -- the project's own code. Only the abstraction headers under
# include/openrider/ are public interface. Third-party libraries come from
# the OpenRiderDependencies framework.
PREREQS=OpenRiderDependencies
# Doxygen of the public plugin ABI; internal headers carry the same
# documentation (coding rules).
# @impl 0f87-6acd-d7ea-8da3
DOCS=yes
# Also document the framework-local and app-private headers, and have
# Doxygen report anything left undocumented.
DOC_INPUT_EXTRA=local/include openrider.m/src
DOC_WARN_IF_UNDOCUMENTED=YES
.include "mk.framework.mk"
