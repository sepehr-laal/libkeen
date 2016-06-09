#include "core.hpp"
#include "curl.hpp"
#include "cache.hpp"
#include "logger.hpp"

namespace {

std::string implode(const std::vector<std::string>& elements, char glue)
{
    switch (elements.size())
    {
    case 0:
        return "";
    case 1:
        return elements[0];
    default:
        std::ostringstream os;
        std::copy(elements.begin(), elements.end() - 1, std::ostream_iterator<std::string>(os, std::string(1, glue).c_str()));
        os << *elements.rbegin();
        return os.str();
    }
}

std::vector<std::string> explode(const std::string &input, char separator)
{
    std::vector<std::string> ret;
    std::string::const_iterator cur = input.begin();
    std::string::const_iterator beg = input.begin();
    bool added = false;
    while (cur < input.end())
    {
        if (*cur == separator)
        {
            ret.insert(ret.end(), std::string(beg, cur));
            beg = ++cur;
            added = true;
        }
        else
        {
            cur++;
        }
    }

    ret.insert(ret.end(), std::string(beg, cur));
    return ret;
}

}

namespace libmetrics {

CoreRef Core::instance(AccessType type)
{
    static CoreRef      core;
    static std::mutex   mutex;

    std::lock_guard<decltype(mutex)> lock(mutex);

    if (type == AccessType::Current)
    {
        /* no-op */
    }
    else if (type == AccessType::Release)
    {
        if (core) core.reset();
    }
    else if (type == AccessType::Renew)
    {
        if (!core) core.reset(new Core);
    }

    return core;
}

CoreRef Core::instance()
{
    if (!instance(AccessType::Current))
        return instance(AccessType::Renew);
    else
        return instance(AccessType::Current);
}

void Core::release()
{
    instance(AccessType::Release);
}

Core::Core()
    : mServiceWork(new asio::io_service::work(mIoService))
    , mCurlRef(std::make_shared<Curl>())
    , mCacheRef(std::make_shared<Cache>())
{
    Logger::pull(mLoggerRefs);
    respawn();
}

void Core::respawn()
{
    LOG_INFO("Resetting IO service.");
    mIoService.reset();

    LOG_INFO("Allocating new work.");
    mServiceWork.reset(new asio::io_service::work(mIoService));

    // hardware_concurrency can return zero, in that case one thread is forced
    unsigned num_threads = std::thread::hardware_concurrency();

    if (num_threads == 0)
    {
        LOG_WARN("hardware_concurrency returned 0. Forcing one thread.");
        num_threads = 1;
    }

    for (unsigned t = 0; t < num_threads; ++t)
    {
        mThreadPool.push_back(std::thread([this] { mIoService.run(); }));
        LOG_INFO("Spawned thread " << mThreadPool.back().get_id());
    }

    LOG_INFO("Thread pool size: " << mThreadPool.size());
}

void Core::shutdown()
{
    if (!mIoService.stopped())
    {
        LOG_INFO("Clearing work.");
        if (mServiceWork) mServiceWork.reset();

        LOG_INFO("Waiting for pending handlers.");
        mIoService.run();

        LOG_INFO("Stopping IO service.");
        mIoService.stop();
    }

    if (!mThreadPool.empty())
    {
        for (auto& thread : mThreadPool)
        {
            LOG_INFO("Shutting down thread " << thread.get_id());
            if (thread.joinable()) thread.join();
        }

        mThreadPool.clear();
        LOG_INFO("Thread pool is empty.");
    }
}

Core::~Core()
{
    shutdown();
    LOG_INFO("Core is shutdown.");
}

void Core::postEvent(
    const std::string& url,
    const std::string& data,
    const std::vector<std::string>& headers)
{
    LOG_INFO("Attempting to post and event to: " << url << " with data: " << data);

    mIoService.post([=]
    {
        auto response = mCurlRef->postData(url, data, headers);

        if (response < 200 || response > 300)
        {
            auto headers_copy = headers;
            headers_copy.push_back(url);
            mCacheRef->push(implode(headers_copy, '\n'), data);

            LOG_WARN("Cached event for: " << url << " and data: " << data);
        }
        else
        {
            LOG_INFO("Sent event for: " << url << " and data: " << data);
        }
    });
}

void Core::postCache(unsigned count)
{
    LOG_INFO("Attempting to post cache with count: " << count);

    mIoService.post([=]
    {
        std::vector<std::pair<std::string, std::string>> caches;
        mCacheRef->pop(caches, count);

        if (caches.empty())
            return;

        for (auto& entry : caches)
        {
            auto entry_decoded = explode(entry.first, '\n');
            auto url = entry_decoded.back();
            entry_decoded.pop_back();

            LOG_DEBUG("Attempting to post cached event to: " << url << " with data: " << entry.second);

            mIoService.post([=]
            {
                auto response = mCurlRef->postData(url, entry.second, entry_decoded);

                if (response < 200 || response > 300)
                {
                    LOG_INFO("Failed sending cached event for: " << url << " and data: " << entry.second);
                }
                else
                {
                    mCacheRef->remove(entry.first, entry.second);
                    LOG_INFO("Cache deleted for: " << url << " and data: " << entry.second);
                }
            });
        }
    });
}

void Core::flush()
{
    LOG_INFO("Flushing the core.");

    shutdown();
    respawn();

    LOG_INFO("Flush finished.");
}

void Core::enableLogToFile(bool on /*= true*/)
{
    for (auto logger : mLoggerRefs)
        if (logger) logger->enableLogToFile(on);
}

void Core::enableLogToConsole(bool on /*= true*/)
{
    for (auto logger : mLoggerRefs)
        if (logger) logger->enableLogToConsole(on);
}

void Core::clearCache()
{
    if (mCacheRef)
        mCacheRef->clear();
}

unsigned Core::useCount()
{
    if (!instance(AccessType::Current)) return 0;
    // minus one to take current method's ref into account
    else return static_cast<unsigned>(instance(AccessType::Current).use_count()) - 1;
}

}
