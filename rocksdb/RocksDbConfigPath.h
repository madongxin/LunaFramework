#pragma once

#include "LunaServerPaths.h"

#include <string>

namespace RocksDbConfigPath {

inline const std::string &Cnf() {
    static std::string path = "../config/rocksdb.cnf";
    static bool once = false;
    if (!once) {
        once = true;
        std::string resolved;
        if (LunaServerPaths::ResolveProjectSubdir("config/rocksdb.cnf", &resolved))
            path = std::move(resolved);
    }
    return path;
}

}  // namespace RocksDbConfigPath
