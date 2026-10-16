# Windows: cJSON is a static, private dependency of the DLLs that embed it
# (libcore, some plugins). Left alone, its header marks every function it
# defines __declspec(dllexport), which turns off the linker's automatic export
# of everything else: libcore.dll then exported only cJSON. Only cJSON's own
# definitions matter -- a consumer merely declaring them exports nothing.
# @impl 1c06-6ad1-21b6-02fe
CFLAGS += -DCJSON_HIDE_SYMBOLS
