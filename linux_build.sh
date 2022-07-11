compiler=gcc

mkdir -p build
rm -r build/*

common_flags="-g -O0"

memory_sources="src/memory.c"
exe_sources="
src/color.c
src/evaluation_graph.c
src/hash.c
src/logging.c
src/main.c
src/memory.c
src/platform_glx.c
src/platform_linux.c
src/plugin_manager.c
src/stretchy_buffer.c
src/util.c
"

libs="-lGLX -lX11 -lGL -lm -ldl"
warnings="-Wall -Wextra -Wpedantic"

exe_flags="-Wl,--export-dynamic"
plugin_flags="-fpic -shared"

cp -r src/shaders build/
cp -r assets build/

build_plugin () {
    $compiler $common_flags $plugin_flags $warnings $2 -o build/lib$1.so
}

$compiler $common_flags $exe_flags $warnings $exe_sources -o build/oui $libs

build_plugin "memory" "src/memory.c"
build_plugin "database" "src/data_model.c"
build_plugin "renderer" "src/renderer.c"
build_plugin "ui" "src/ui.c"
