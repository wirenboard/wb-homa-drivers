#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "utils.h"

using namespace std;



time_t m_lasttime=0;


time_t mytime(time_t * _Time)
{
	time_t acttime=time(_Time);
	if (acttime<m_lasttime)
		return m_lasttime;
	m_lasttime=acttime;
	return acttime;
}

bool isInt(const std::string &s)
{
	for(size_t i = 0; i < s.length(); i++){
		if(!isdigit(s[i]))
			return false;
	}
	return true;
}

bool StringHasSuffix(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}
bool StringStartsWith(const string& str, const string& prefix) {
    return prefix.size() <= str.size() && str.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> StringSplit(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
std::vector<std::string> StringSplit(const std::string &s, const std::string& delim)
{
    std::vector<std::string> elems;

    size_t current;
    size_t delim_size = delim.size();
    size_t next = -delim_size;

    do {
      current = next + delim_size;
      next = s.find( delim, current );
      elems.push_back(s.substr( current, next - current ));
    } while (next != string::npos);

    return elems;
}

std::string StringReplace(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

void StringUpper(std::string& str)
{
   for (std::string::iterator p = str.begin(); str.end() != p; ++p) {
          *p = toupper(*p);
          //~ std::cout << *p << std::endl;
   }
}
