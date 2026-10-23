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
DOC_INPUT_EXTRA=local/include openrider.m/src libreproj.m/src tiles_wmts.m/src tiles_xyz.m/src filter_jitter.m/src input_garmindb.m/src input_gpx.m/include input_gpx.m/src segments_heatmap.m/include segments_heatmap.m/src segments_matching.m/include segments_matching.m/src
DOC_WARN_IF_UNDOCUMENTED=YES
.include "mk.framework.mk"
