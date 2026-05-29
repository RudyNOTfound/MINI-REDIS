#pragma once
#include <string>
#include <fstream>

class Store; 

class WAL
{
public:
    WAL(const std::string &path);
    ~WAL();
    void log(const std::string &line); 
    void replay(Store &db);            
    void clear();                      

private:
    std::string path_;
    std::ofstream out_;
    int write_count_ = 0; // tracks writes between flushes
};