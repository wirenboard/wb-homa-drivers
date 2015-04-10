#pragma once
#include <string>

#include <curl/curl.h>

//~ #include "common/utils.h"

using namespace std;


// FIXME: use TMaybe and hide implementation (curl) details
CURLcode GetHttpUrl(const std::string& url, std::ostream& os, long timeout = 30);
bool GetHTTPUrl(const std::string& url, std::string& result);

