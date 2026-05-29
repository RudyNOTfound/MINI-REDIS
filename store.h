#pragma once
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>

// each key stores its value, when it dies, and where it sits in the LRU list
// lru_it is an iterator into lru_list — lets us move a node to front in O(1)
// without it we'd have to scan the whole list every time (O(n)), which kills perf
struct Entry
{
    std::string value;
    long long expire_at;                     // unix timestamp, -1 means no expiry
    std::list<std::string>::iterator lru_it; // direct pointer into lru_list
};

// stripped down version of Entry for snapshots
// can't use Entry directly because lru_it points into a live list
// copying it across threads = segfault (learnt the hard way)
struct SnapEntry
{
    std::string value;
    long long expire_at;
};

class Store
{
public:
    Store(int max_keys = 1000);

    void set(const std::string &key, const std::string &val, int ttl = -1);
    std::string get(const std::string &key);
    int del(const std::string &key);
    int size();

    // setRaw is only for loading snapshots — bypasses TTL calculation
    // since expire_at is already an absolute timestamp, not a relative TTL
    void setRaw(const std::string &key, const std::string &val, long long expire_at);
    std::unordered_map<std::string, SnapEntry> getAll();

private:
    std::unordered_map<std::string, Entry> data;
    std::list<std::string> lru_list; // front = most recent, back = evict next
    int max_keys_;
    std::mutex mtx; // one lock for everything — simple but it works
    long long now();
    void touch(const std::string &key);
    void evict();
};