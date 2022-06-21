compiler=gcc

mkdir -p build
libs="-lGLX -lX11 -lGL"
warnings="-Wall -Wextra -Wpedantic"
$compiler $warnings src/main.c src/platform_glx.c -o build/mygui $libs
