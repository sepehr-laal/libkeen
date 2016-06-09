#include "curl.hpp"
#include "scoped.hpp"
#include "logger.hpp"

#include <curl/curl.h>

namespace libmetrics {

class LibCurlHandle
{
public:
    static std::shared_ptr< LibCurlHandle > ref() {
        static std::shared_ptr< LibCurlHandle > instance{ new LibCurlHandle };
        return instance;
    }

    LibCurlHandle();
    ~LibCurlHandle();

    bool                            isReady() const;
    const std::vector<std::string>& getDefaultHeaders() const;

private:
    bool                            mReady = false;
    std::vector<std::string>        mDefaultHeaders;
    std::vector<LoggerRef>          mLoggerRefs;
};

LibCurlHandle::LibCurlHandle()
{
    LOG_INFO("Starting up cURL");
    Logger::pull(mLoggerRefs);
    mReady = (::curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
}

LibCurlHandle::~LibCurlHandle()
{
    LOG_INFO("Shutting down cURL");
    if (isReady()) { ::curl_global_cleanup(); }
}

bool LibCurlHandle::isReady() const
{
    return mReady;
}

const std::vector<std::string>& LibCurlHandle::getDefaultHeaders() const
{
    return mDefaultHeaders;
}

namespace {
static std::size_t write_cb(void *contents, std::size_t size, std::size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}}

Curl::Curl()
    : mLibCurlHandle(LibCurlHandle::ref())
{
    Logger::pull(mLoggerRefs);

    if (mLibCurlHandle && mLibCurlHandle->isReady())
        LOG_INFO("cURL is initialized successfully.");
}

short Curl::postData(
    const std::string& url,
    const std::string& data)
{
    if (mLibCurlHandle)
        return postData(url, data, mLibCurlHandle->getDefaultHeaders(), nullptr);
    else
        return 0;
}

short Curl::postData(
    const std::string& url,
    const std::string& data,
    const std::vector<std::string>& headers,
    std::string* reply /*= nullptr*/)
{
    if (!mLibCurlHandle || !mLibCurlHandle->isReady())
    {
        LOG_WARN("cURL is not ready. Invalid operation.");
        return 0;
    }

    if (auto curl = ::curl_easy_init())
    {
        curl_slist *curl_headers = nullptr;

        Scoped<CURL>        scope_bound_curl(curl);
        Scoped<curl_slist>  scope_bound_slist(curl_headers);

        if (!headers.empty())
        {
            for (const auto& header : headers)
                curl_headers = curl_slist_append(curl_headers, header.c_str());

            ::curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
        }

        ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        ::curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

        if (reply != nullptr)
        {
            if (!reply->empty()) reply->clear();
            ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
            ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, reply);
        }

        LOG_INFO("cURL is about to post to: " << url << " with data: " << data);

        if (curl_easy_perform(curl) == CURLE_OK)
        {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

            LOG_INFO(data << " was sent to " << url << " with status code: " << http_code);

            return static_cast<short>(http_code);
        }
        else
        {
            LOG_ERROR("curl_easy_perform failed");
        }
    }
    else
    {
        LOG_ERROR("curl_easy_init failed");
    }

    return 0;
}

}
