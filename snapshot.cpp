#include "snapshot.h"
#include <fstream>
#include <sstream>
using namespace std;

// dumps entire store to disk as tab-separated lines
// format: key \t value \t expire_at
// after a snapshot, the WAL can be cleared — this file replaces it
void saveSnapshot(Store &db, const string &path)
{
    auto entries = db.getAll(); // safe copy, no live iterators
    ofstream out(path, ios::trunc);
    for (auto &[key, entry] : entries)
    {
        out << key << "\t"
            << entry.value << "\t"
            << entry.expire_at << "\n";
    }
    out.flush();
}

// reads snapshot back into store on startup
// runs before WAL replay so WAL can overwrite with any changes made after snapshot
void loadSnapshot(Store &db, const string &path)
{
    ifstream in(path);
    if (!in.is_open())
        return;

    string line;
    while (getline(in, line))
    {
        if (line.empty())
            continue;
        istringstream ss(line);
        string key, val;
        long long exp;
        getline(ss, key, '\t');
        getline(ss, val, '\t');
        ss >> exp;
        db.setRaw(key, val, exp); // setRaw takes absolute timestamp, not TTL
    }
}