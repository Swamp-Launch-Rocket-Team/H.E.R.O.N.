#pragma once
// map_image.h: the ground-track background map as a raylib texture.
#include <fstream>
#include <raylib.h>
#ifndef STB_IMAGE_STATIC
#define STB_IMAGE_STATIC   // raylib exports its own stbi_* functions; keep display.cpp's copy private
#endif
#include <stb_image.h>
#include <map_box.h>

struct MapImage{
    Texture2D tex{};   // tex.id == 0 until a map is loaded
    int width = 0, height = 0;
    double north = 0, south = 0, east = 0, west = 0;
};
// Loads the JPG with stb (raylib's own JPG loader is off by default) and uploads it as a texture.
// Call only after the raylib window exists.
inline bool fillMapImage(MapImage& image, const char* jpg_path, const char* txt_path){
    std::ifstream file(txt_path);
    if(!file) return false;
    file >> image.west >>image.south >>image.east>>image.north;
    if(!file) return false;
    int channels = 0;
    unsigned char* pixels = stbi_load(jpg_path, &image.width, &image.height, &channels, 4);
    if(!pixels) return false;
    Image img{};
    img.data = pixels;
    img.width = image.width;
    img.height = image.height;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.tex = LoadTextureFromImage(img);   // copies the pixels to the GPU
    stbi_image_free(pixels);
    SetTextureFilter(image.tex, TEXTURE_FILTER_BILINEAR);
    return image.tex.id != 0;
}
