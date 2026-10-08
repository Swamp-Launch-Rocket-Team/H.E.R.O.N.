#pragma once
#include "line_reader.h"
#include <filesystem>
#include <cstdio>
#include <string>

class LogTail{
    std::FILE* file = nullptr;
    std::string path; //log that currently open
    LineReader reader;

    public:
    bool follow_newest(const std::string& folder); // open the newest *_raw.log if it isn't already open
    bool next_line(std::string& out);
    const std::string& current() const { return path;}
    ~LogTail();
};

inline LogTail::~LogTail(){
    if(file) std::fclose(file);
};
inline bool LogTail::follow_newest(const std::string& folder){
    std::error_code ec;
    std::string best; // biggest *_raw.log name so far
    for(const auto& entry : std::filesystem::directory_iterator(folder, ec)){
        std::string name = entry.path().filename().string();
        if(!(name.size() >= 8 && name.compare(name.size() - 8, 8, "_raw.log") == 0)) continue;
        if(name > best) best = name;
    }
    if(best.empty()) return file != nullptr;
    std::string full = folder + "/" + best;
    if(full == path) return true;
    else{
        if(file) std::fclose(file);
        file = std::fopen(full.c_str(),"rb");
        path = full;
        reader = LineReader();
        return file != nullptr;
    }
    
}
inline bool LogTail::next_line(std::string& out){
    if(!file) return false;
    if(reader.next_line(out)) return true;
    char buf[4096];
    size_t got = std::fread(buf, 1, sizeof buf, file);   // up to 4096 items of 1 byte each, into buf
    reader.feed(buf, got);
    std::clearerr(file);
    return reader.next_line(out);   
}