# Framework-wide post hook. A module declaring PLUGIN_KIND=<kind> is a
# plugin: once built, its shared library is installed into the workspace's
# build/<KEY>/plugins/<kind>/, next to bin/, where the application looks.
.if defined(PLUGIN_KIND) && !empty(PLUGIN_KIND) && defined(SHLIB_NAME)
_PLUGIN_DST = ${.CURDIR}/../../build/${_TARGET_KEY}/plugins/${PLUGIN_KIND}
all: _install_plugin
_install_plugin: .PHONY ${_LIBOUT_DIR}/${SHLIB_NAME}
	@mkdir -p ${_PLUGIN_DST}
	@# Copy then rename: overwriting a loaded/signed dylib in place makes
	@# macOS kill the next process that maps it (stale code signature).
	@cp -f ${_LIBOUT_DIR}/${SHLIB_NAME} ${_PLUGIN_DST}/.${SHLIB_NAME}.tmp && \
		mv -f ${_PLUGIN_DST}/.${SHLIB_NAME}.tmp ${_PLUGIN_DST}/${SHLIB_NAME}
	@echo "===> installed plugin ${SHLIB_NAME} -> plugins/${PLUGIN_KIND}/"
.endif
