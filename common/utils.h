#pragma once
#include <string>
#include <vector>
#include <memory>
#include <time.h>

using namespace std;




time_t mytime(time_t * _Time);
bool isInt(const std::string &s);
bool StringHasSuffix(const std::string &str, const std::string &suffix);
bool StringStartsWith(const string& str, const string& prefix);
std::vector<std::string> StringSplit(const std::string &s, char delim);
std::vector<std::string> StringSplit(const std::string &s, const std::string& delim);

std::string StringReplace(std::string subject, const std::string& search,
                          const std::string& replace);


void StringUpper(std::string& str);


class TBaseException : public std::exception
{
public:
   TBaseException(const std::string& message) : Message(message) {}
   virtual ~TBaseException() throw() {}
   virtual const char* what() const throw() {return Message.c_str();}
protected:
   std::string Message;
};

class TMaybeNotDefined_ {};
#define NotDefinedMaybe TMaybeNotDefined_();

template<typename T>
class TMaybe
{
public:
    inline TMaybe(const T& val) {Init(val);}
    inline TMaybe<T>& operator=(const T& val) { Init(val); return *this; }

    inline TMaybe() {};
    inline TMaybe(const TMaybeNotDefined_&) {};


    inline bool Defined() const { return !!Data_; }
    inline bool Empty() const { return !Data_(); }
    bool operator !() const { return Empty(); }


    const T* Get() const { return Defined() ? Data() : nullptr; }
    T* Get() { return Defined() ? Data() : nullptr; }

    inline const T& GetRef() const { EnsureDefined(); return *Data(); }
    inline T& GetRef() { EnsureDefined();  return *Data(); }

    inline const T& operator*() const { return GetRef();}
    inline T& operator*() { return GetRef(); }

    const T* operator->() const {  return &GetRef();}
    inline T* operator->() { return &GetRef(); }


    inline void EnsureDefined() const {
        if (!Defined()) {
            throw TBaseException("TMaybe is not defined");
        }
    }


private:
    inline void Init(const T& val) { Data_.reset(new T(val)); };
    inline T* Data() const { return Data_.get(); };


    unique_ptr<T> Data_;
};



