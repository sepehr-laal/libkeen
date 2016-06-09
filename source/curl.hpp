#pragma once

#include "fwd.hpp"

namespace libmetrics {

/*!
 * @class Curl
 * @brief Handles interfacing with cURL.
 * @note  Realistically only one instance of this class
 *        Exists at a time and Core owns it.
 */
class Curl
{
public:
    Curl();
    
    //! posts "data" to "url" via HTTP POST, returns HTTP response code
    short postData(
        const std::string& url,
        const std::string& data);

    //! posts "data" to "url" with "headers" via HTTP POST, returns HTTP
    //! response code and fills "reply" with the HTTP response body
    short postData(
        const std::string& url,
        const std::string& data,
        const std::vector<std::string>& headers,
        std::string* reply = nullptr);

private:
    std::shared_ptr< class LibCurlHandle >
        mLibCurlHandle;
    std::vector< LoggerRef >
        mLoggerRefs;
};

}
