# OpenRider -- Bmake It workspace (https://github.com/SylvainSouche/bmake-it).
# bmake must find Bmake It's mk/ directory: run its scripts/install-env.sh
# once, or set MAKESYSPATH=<bmake-it>/mk:<default>.
PARENT_WS=
.include "mk.workspace.mk"
