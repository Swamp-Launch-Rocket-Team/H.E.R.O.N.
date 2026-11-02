#pragma once
#include <GLFW/glfw3.h>
#include <fstream>
#include <string>
#include <cstdlib>
#include <stb_image.h>

struct MapImage{
    GLuint tex = 0;
    int width = 0, height = 0;
    double north = 0, south = 0, east = 0, west = 0;
};
inline bool fillMapImage(MapImage& image, const char* jpg_path, const char* txt_path){
    std::ifstream file(txt_path);
    if(!file) return false;
    file >> image.west >>image.south >>image.east>>image.north;
    if(!file) return false;
    int channels = 0;
    unsigned char* pixels = stbi_load(jpg_path, &image.width, &image.height, &channels, 4);
    if(!pixels) return false;
    glGenTextures(1, &image.tex);
    glBindTexture(GL_TEXTURE_2D, image.tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    stbi_image_free(pixels);
    

    return true;
}