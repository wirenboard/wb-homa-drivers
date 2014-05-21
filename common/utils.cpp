#include <string>
#include <vector>
#include <sstream>
#include "utils.h"

using namespace std;




bool has_suffix(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}
bool starts_with(const string& str, const string& prefix) {
    return prefix.size() <= str.size() && str.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
