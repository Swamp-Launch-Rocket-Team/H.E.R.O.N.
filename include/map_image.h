#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GLFW/glfw3.h>
#include <fstream>
#include <string>
#include <cstdlib>
#include <stb_image.h>
#include <urlmon.h>
#include <iomanip>
#include <flight_stats.h>


const double MAP_RADIUS_M = 4000;
const int MAP_GRID = 2;
const int MAP_MAX_PX = 4096;
struct MapImage{
    GLuint tex = 0;
    int width = 0, height = 0;
    double north = 0, south = 0, east = 0, west = 0;
};
struct MapBox { double west, south, east, north; };
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

inline MapBox map_box_around(double pad_lat, double pad_lon){
    MapBox box;
    double dlat =  MAP_RADIUS_M / 111320;
    double dlon = MAP_RADIUS_M / (111320 * std::cos(deg_to_rad(pad_lat)));
    box.north = pad_lat + dlat;
    box.south = pad_lat - dlat;
    box.east = pad_lon + dlon;
    box.west = pad_lon - dlon;
    return box;
}
inline bool download_map(MapBox box, const char* jpg_path, const char* txt_path){
    int height = round(MAP_MAX_PX *(box.north - box.south)/(box.east - box.west));
    int width = MAP_MAX_PX;
    char url[512];
    std::snprintf(url, sizeof url,
        "https://basemap.nationalmap.gov/arcgis/rest/services/USGSImageryOnly/MapServer/export"
        "?bbox=%.9f,%.9f,%.9f,%.9f&bboxSR=4326&imageSR=4326&size=%d,%d&format=jpg&f=image",
        box.west, box.south, box.east, box.north, width, height);
    HRESULT r = URLDownloadToFileA(nullptr, url, jpg_path, 0, nullptr);
    if(r != S_OK){
      std::fprintf(stderr, "map download failed (0x%08lx): %s\n", (unsigned long)r, url); 
      return false;
    } 
    std::ofstream file(txt_path);
    if(!file) return false;
    file << std::setprecision(12)<< box.west << "\n"<<box.south<<"\n"<<box.east <<"\n"<<box.north;
    return true;
}