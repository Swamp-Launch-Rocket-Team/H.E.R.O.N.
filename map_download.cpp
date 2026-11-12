// map_download.cpp: the USGS map download, in its own file because <urlmon.h> pulls in
// windows.h, which defines CloseWindow, DrawText, LoadImage, Rectangle and ShowCursor:
// the same names raylib uses. Never include raylib.h (or map_image.h) here.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <urlmon.h>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include "map_box.h"

bool download_map(MapBox box, const char* jpg_path, const char* txt_path){
    int height = (int)std::round(MAP_MAX_PX *(box.north - box.south)/(box.east - box.west));
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
