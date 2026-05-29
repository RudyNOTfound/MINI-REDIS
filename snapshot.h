#pragma once
#include <string>
#include "store.h"
using namespace std;

void saveSnapshot(Store &db, const string &path);
void loadSnapshot(Store &db, const string &path);