# Reprojection: coordinate systems declared as text (EPSG code, area of use,
# PROJ string) over the projection methods coded here, datum shifts
# (7-parameter Helmert), batch transformation of vertices, and raster
# warping -- as a triangle mesh for the GPU to draw (the hardware path) or
# on the CPU (fallback, export, tests). Independent: no other OpenRider
# module, no third-party library. ETRS89 and its national realisations are
# taken as WGS 84 (under a metre apart).
# @impl 0f87-6ada-4bb4-1ee9
# @impl 0f87-6ada-48b6-11bc
LIB=reproj
LIB_SHARED=NO
TESTS_CXX=projection_test registry_test transformer_test warp_mesh_test raster_warp_test grid_projector_test
# The tests check the definitions OpenRider ships.
CXXFLAGS+=-DREPROJ_CRS_FILE=\"${.CURDIR}/../share/common/openrider/crs.txt\"
.include "mk.lib.mk"
