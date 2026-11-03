# A plugin's unit tests reach its internals, while the plugin itself exports
# only openrider_plugin (OR_PLUGIN_EXPORT): they link its objects, not its DLL.
.if !empty(PLUGIN_KIND)
TEST_LINK = objects
.endif
