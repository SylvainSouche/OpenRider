# Install the font into the framework's share/, copied up to the workspace.
_FONT_DST = ${.CURDIR}/../build/${_TARGET_KEY}/share/openrider/fonts
all: _install_font
_install_font: .PHONY _fetch_import
	@mkdir -p ${_FONT_DST}
	@cp -f ${_IMPORT_WRKSRC}/src/regular/Phosphor.ttf ${_FONT_DST}/
	@echo "===> installed font Phosphor.ttf -> share/openrider/fonts/"
