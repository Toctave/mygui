#include "color.h"

color_t color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    color_t result;
    result.rgba[0] = r;
    result.rgba[1] = g;
    result.rgba[2] = b;
    result.rgba[3] = a;

    return result;
}

color_t color_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return color_rgba(r, g, b, 0xFF);
}

color_t color_gray(uint8_t w) { return color_rgb(w, w, w); }
