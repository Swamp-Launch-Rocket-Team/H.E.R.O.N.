#pragma once
#include <string>
#include <cstddef>

class LineReader{
    private:
        std::string buffer;
        size_t droppedLines = 0;
        
    public:
        size_t dropped_Count() const{
            return droppedLines;
        }

        void feed(const char* data, size_t n){
            buffer.append(data,n);
            return;
        }
        //adds the next line to the output
        bool next_line(std::string& out){
            while(true){
                size_t position = buffer.find('\n');
                if(position == std::string::npos){
                    if(buffer.size() > 512){
                        buffer.clear();
                        droppedLines++;
                        
                    }
                    return false;
                }
                if(position > 512 ){
                    buffer.erase(0,position+1);
                    droppedLines++;
                    continue;
                } 
                out = buffer.substr(0,position);
                if(!out.empty() && out.back() == '\r') out.pop_back();
                buffer.erase(0,position+1);
                if(out.empty()) continue;
                else return true;
            }
            
        }
    
};

