#pragma once
#include <string>
#include <unordered_map>
#include <mutex> 

// One value stored in our database
struct Entry {
    std::string value;
    long long   expire_at;   // unix seconds, -1 = never expires
};

// The store — one class that wraps everything
class Store {
public:
    void        set(const std::string& key, const std::string& val, int ttl = -1);
    std::string get(const std::string& key);
    int         del(const std::string& key);
    int         size();

private:
    std::unordered_map<std::string, Entry> data;   // THE hashmap
    std::mutex  mtx;  
    long long now();
};