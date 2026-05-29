#include "wal.h"
#include "store.h"
#include <fstream>
#include <sstream>

WAL::WAL(const std::string &path) : path_(path)
{
    out_.open(path, std::ios::app); // append — never blow away existing log
}

WAL::~WAL()
{
    if (out_.is_open())
        out_.close();
}

void WAL::log(const std::string &line)
{
    if (!out_.is_open())
        return;
    out_ << line << "\n";
    write_count_++;
    // flush every 100 writes instead of every single one
    // flushing forces the OS to write to disk immediately
    // but disk I/O is slow — per-write flush was bottlenecking SET at 2k req/sec
    // every-100 flush gives 8x better throughput with acceptable crash risk
    // (worst case: lose last 100 writes on a crash)
    if (write_count_ % 100 == 0)
    {
        out_.flush();
        write_count_ = 0;
    }
}

// called once on startup — rebuilds in-memory state from the log file
void WAL::replay(Store &db)
{
    std::ifstream in(path_);
    if (!in.is_open())
        return; // no log file = fresh start, nothing to replay

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;
        std::istringstream ss(line);
        std::string op, key, val;
        int ttl = -1;
        ss >> op >> key;
        if (op == "SET")
        {
            ss >> val >> ttl;
            db.set(key, val, ttl);
        }
        else if (op == "DEL")
        {
            db.del(key);
        }
    }
}

// wipe the log after a snapshot — snapshot already has everything
// no point keeping old entries around
void WAL::clear()
{
    if (out_.is_open())
        out_.close();
    out_.open(path_, std::ios::trunc);
}