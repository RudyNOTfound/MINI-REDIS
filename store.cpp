#include "store.h"
#include <chrono>


// In store.cpp — implement them:
void Store::setRaw(const std::string& key, const std::string& val, long long expire_at) {
    std::lock_guard<std::mutex> lock(mtx);
    data[key] = {val, expire_at};
}

std::unordered_map<std::string, Entry> Store::getAll() {
    std::lock_guard<std::mutex> lock(mtx);
    return data;   // returns a copy — safe to use outside the lock
}

// Current unix timestamp in seconds
long long Store::now() {
    using namespace std::chrono;
    return duration_cast<seconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

// SET key value [ttl]
void Store::set(const std::string& key, const std::string& val, int ttl) {
    std::lock_guard<std::mutex> lock(mtx);
    long long exp = (ttl > 0) ? now() + ttl : -1;
    data[key] = {val, exp};
}



// GET key → value string, or "(nil)"
std::string Store::get(const std::string& key) {
    auto it = data.find(key);

    // Key doesn't exist
    if (it == data.end()) return "(nil)";

    Entry& e = it->second;

    // Lazy TTL check — expired?
    if (e.expire_at != -1 && now() > e.expire_at) {
        data.erase(it);    // clean it up
        return "(nil)";
    }

    return e.value;
}

// DELETE key → 1 if deleted, 0 if not found
int Store::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx);
    return data.erase(key);   // erase() returns count removed
}

// How many keys are currently stored
int Store::size() {
    std::lock_guard<std::mutex> lock(mtx);
    return data.size();
}