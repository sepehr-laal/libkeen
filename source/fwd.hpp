#pragma once

#include <sstream>
#include <vector>
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

struct sqlite3;

namespace libmetrics
{

class Core;
class Curl;
class Cache;
class Logger;

using CoreRef   = std::shared_ptr< Core >;
using CurlRef   = std::shared_ptr< Curl >;
using CacheRef  = std::shared_ptr< Cache >;
using LoggerRef = std::shared_ptr< Logger >;

}
