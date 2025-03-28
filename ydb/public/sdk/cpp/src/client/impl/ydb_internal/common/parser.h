#pragma once

#include <string>

namespace NYdb::inline Dev {

struct TConnectionInfo {
    std::string Endpoint = "";
    std::string Database = "";
    bool EnableSsl = false;
};

TConnectionInfo ParseConnectionString(const std::string& connectionString);

} // namespace NYdb

