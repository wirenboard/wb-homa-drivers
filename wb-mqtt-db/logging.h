#pragma once

#include <log4cpp/Category.hh>

#define LOG(x) log4cpp::Category::getRoot() << log4cpp::Priority::x 
#define SYSLOG(x) LOG(x)
#define VLOG(x) LOG(DEBUG) /* TODO: deal with this */

// This class magic is from gLog, look there
class LogMessageVoidify {
public:
    LogMessageVoidify() {}

    void operator&(const log4cpp::CategoryStream &) {}
};

#ifndef NDEBUG
    #define DVLOG(x) VLOG(x)
    #define DLOG(x) LOG(x)
#else
    #define DVLOG(x) (true) ? (void) 0 : LogMessageVoidify() & LOG(INFO)
    #define DLOG(x) (true) ? (void) 0 : LogMessageVoidify() & LOG(x)
#endif
