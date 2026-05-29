#pragma once
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
using namespace std;

// each key stores its value, when it dies, and where it sits in the LRU list
// lru_it is an iterator into lru_list — lets us move a node to front in O(1)
// without it we'd have to scan the whole list every time (O(n)), which kills perf
struct Entry
{
    string value;
    long long expire_at;           // unix timestamp, -1 means no expiry
    list<string>::iterator lru_it; // direct pointer into lru_list
};

// stripped down version of Entry for snapshots
// can't use Entry directly because lru_it points into a live list
// copying it across threads = segfault (learnt the hard way)
struct SnapEntry
{
    string value;
    long long expire_at;
};

class Store
{
public:
    Store(int max_keys = 1000);

    void set(const string &key, const string &val, int ttl = -1);
    string get(const string &key);
    int del(const string &key);
    int size();

    // setRaw is only for loading snapshots — bypasses TTL calculation
    // since expire_at is already an absolute timestamp, not a relative TTL
    void setRaw(const string &key, const string &val, long long expire_at);
    unordered_map<string, SnapEntry> getAll();

private:
    unordered_map<string, Entry> data;
    list<string> lru_list; // front = most recent, back = evict next
    int max_keys_;
    mutex mtx; // one lock for everything — simple but it works
    long long now();
    void touch(const string &key);
    void evict();
};