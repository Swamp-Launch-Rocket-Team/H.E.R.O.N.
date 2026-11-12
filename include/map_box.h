#pragma once
// map_box.h: the map's bounding box and the download declaration.
// No raylib.h and no windows.h here: map_download.cpp includes this next to <urlmon.h>,
// and map_image.h includes it next to raylib.h. The two must never meet in one file.
#include <cmath>
#include <flight_stats.h>

const double MAP_RADIUS_M = 4000;
const int MAP_GRID = 2;
const int MAP_MAX_PX = 4096;

struct MapBox { double west, south, east, north; };

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

// Downloads a USGS imagery JPG of `box` to jpg_path and writes its bounds to txt_path.
// Defined in map_download.cpp, because it needs windows.h.
bool download_map(MapBox box, const char* jpg_path, const char* txt_path);
