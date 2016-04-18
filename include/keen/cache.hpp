#pragma once

#include <memory>
#include <vector>
#include <string>

struct sqlite3;

namespace libkeen {
namespace internal {

using Sqlite3Ref = std::shared_ptr< class Cache >;

class Cache
{
public:
    ~Cache();
    static Sqlite3Ref   ref();
    void                push(const std::string& name, const std::string& event);
    void                pop(std::vector<std::pair<std::string, std::string>>& records, unsigned count);
    void                remove(const std::string& name, const std::string& event);

private:
    Cache();
    sqlite3             *mConnection;
};

}}
