#include "store.h"
#include <chrono>

Store::Store(int max_keys) : max_keys_(max_keys) {}

long long Store::now()
{
    using namespace std::chrono;
    return duration_cast<seconds>(
               system_clock::now().time_since_epoch())
        .count();
}

// moves a key to the front of lru_list
// this is why we store the iterator in Entry — erase + push_front is O(1)
// if we stored the key's index instead, finding it would be O(n)
void Store::touch(const std::string &key)
{
    auto it = data.find(key);
    if (it == data.end())
        return;
    lru_list.erase(it->second.lru_it);
    lru_list.push_front(key);
    it->second.lru_it = lru_list.begin();
}

// kick out whoever hasn't been used in the longest time
void Store::evict()
{
    if (lru_list.empty())
        return;
    std::string lru_key = lru_list.back();
    lru_list.pop_back();
    data.erase(lru_key);
}

void Store::set(const std::string &key, const std::string &val, int ttl)
{
    std::lock_guard<std::mutex> lock(mtx);
    // if key already exists, clean it out of lru_list before reinserting
    // otherwise lru_list and data go out of sync and everything breaks
    auto it = data.find(key);
    if (it != data.end())
    {
        lru_list.erase(it->second.lru_it);
        data.erase(it);
    }

    if ((int)data.size() >= max_keys_)
        evict();

    lru_list.push_front(key);
    long long exp = (ttl > 0) ? now() + ttl : -1;
    data[key] = {val, exp, lru_list.begin()};
}

std::string Store::get(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end())
        return "(nil)";

    Entry &e = it->second;
    // lazy expiry — we don't run a background cleaner
    // just check on read and delete if expired
    // real Redis does the same thing
    if (e.expire_at != -1 && now() > e.expire_at)
    {
        lru_list.erase(e.lru_it);
        data.erase(it);
        return "(nil)";
    }

    touch(key); // accessing a key resets its eviction timer
    return e.value;
}

int Store::del(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx);
    auto it = data.find(key);
    if (it == data.end())
        return 0;
    lru_list.erase(it->second.lru_it);
    data.erase(it);
    return 1;
}

int Store::size()
{
    std::lock_guard<std::mutex> lock(mtx);
    return data.size();
}

// used when loading from snapshot
// has to properly initialize lru_it — if you skip this, touch() will
// try to erase a garbage iterator and segfault immediately
void Store::setRaw(const std::string &key, const std::string &val, long long expire_at)
{
    std::lock_guard<std::mutex> lock(mtx);

    // Remove existing entry if key already exists
    auto existing = data.find(key);
    if (existing != data.end())
    {
        lru_list.erase(existing->second.lru_it);
        data.erase(existing);
    }

    // Add to LRU list exactly like set() does
    lru_list.push_front(key);
    data[key] = {val, expire_at, lru_list.begin()};
}

// returns a safe copy for snapshotting
// deliberately excludes lru_it — that iterator is only valid on the live list
std::unordered_map<std::string, SnapEntry> Store::getAll()
{
    std::lock_guard<std::mutex> lock(mtx);
    std::unordered_map<std::string, SnapEntry> result;
    for (auto &[key, entry] : data)
    {
        result[key] = {entry.value, entry.expire_at};
    }
    return result;
}
