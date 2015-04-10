#include <curl/curl.h>
#include "http_helper.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

// http://www.cplusplus.com/forum/unices/45878/

// callback function writes data to a std::ostream
static size_t _CurlDataWriteCallback(void* buf, size_t size, size_t nmemb, void* userp)
{
	if(userp)
	{
		std::ostream& os = *static_cast<std::ostream*>(userp);
		std::streamsize len = size * nmemb;
		if(os.write(static_cast<char*>(buf), len))
			return len;
	}

	return 0;
}

/**
 * timeout is in seconds
 **/
CURLcode GetHttpUrl(const std::string& url, std::ostream& os, long timeout)
{
	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_easy_init();

	if (curl) {
		if (CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &_CurlDataWriteCallback))
            && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L))
            && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
            && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))
            && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))
            && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
		{
			code = curl_easy_perform(curl);
		}
		curl_easy_cleanup(curl);
	}
	return code;
}

bool GetHTTPUrl(const std::string& url, std::string& result)
{
    bool bret;
    std::ostringstream output;
    bret = (GetHttpUrl(url, output, 10) == CURLE_OK);
    result = output.str();
    return bret;
}
