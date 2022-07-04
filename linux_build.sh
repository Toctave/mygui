compiler=gcc

mkdir -p build

common_flags="-g -O0"

memory_sources="src/memory.c"
exe_sources="src/main.c \
src/logging.c \
src/memory.c \
src/platform_glx.c \
src/platform_linux.c \
src/hash.c \
src/data_model.c \
src/util.c \
src/plugin_manager.c \
src/stretchy_buffer.c"

libs="-lGLX -lX11 -lGL -lm -ldl"
warnings="-Wall -Wextra -Wpedantic"

exe_flags="-Wl,--export-dynamic"
plugin_flags="-fpic -shared"

cp -r src/shaders build/
cp -r assets build/
$compiler $common_flags $exe_flags $warnings $exe_sources -o build/oui $libs
$compiler $common_flags $plugin_flags $warnings $memory_sources -o build/libmemory.so
