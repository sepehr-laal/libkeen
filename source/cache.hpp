#pragma once

#include "fwd.hpp"

namespace libmetrics {

class Cache final
{
public:
    Cache();
    ~Cache();

    void push(const std::string& url, const std::string& data);
    bool exists(const std::string& url, const std::string& data) const;
    void pop(std::vector<std::pair<std::string, std::string>>& records, unsigned count) const;
    void remove(const std::string& url, const std::string& data);
    bool connected() const;
    void clear();
    int  count() const;

private:
    sqlite3*                mConnection;
    mutable std::mutex      mCommandLock;
    std::vector<LoggerRef>  mLoggerRefs;
};

}
