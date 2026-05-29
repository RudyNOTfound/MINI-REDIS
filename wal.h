#pragma once
#include <string>
#include <fstream>
using namespace std;

class Store;

class WAL
{
public:
    WAL(const string &path);
    ~WAL();
    void log(const string &line);
    void replay(Store &db);
    void clear();

private:
    string path_;
    ofstream out_;
    int write_count_ = 0; // tracks writes between flushes
};