compiler=gcc

mkdir -p build

common_flags="-g"
sources="src/main.c src/logging.c src/memory.c src/platform_glx.c src/platform_linux.c"
libs="-lGLX -lX11 -lGL -lm"
warnings="-Wall -Wextra -Wpedantic"

cp -r src/shaders build/
cp -r assets build/
$compiler $common_flags $warnings $sources -o build/mygui $libs
