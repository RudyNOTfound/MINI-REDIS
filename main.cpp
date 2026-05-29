#include <iostream>
#include "store.h"

int main()
{
    Store db;

    db.set("name", "alice");
    db.set("city", "delhi");
    db.set("session", "tok123", 3);

    std::cout << db.get("name") << "\n";
    std::cout << db.get("missing") << "\n";
    std::cout << db.size() << "\n";

    db.del("city");
    std::cout << db.get("city") << "\n";
    std::cout << db.size() << "\n";

    return 0;
}