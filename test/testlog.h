#pragma once

#include <sstream>
#include <string>
#include <functional>
#include <gtest/gtest.h>

class TTestLogItem
{
    typedef std::function<void(const std::string&)> TAction;
public:
    TTestLogItem(const TTestLogItem&) {}
    TTestLogItem& operator =(const TTestLogItem&) { return *this; }

    ~TTestLogItem()
    {
        if (Contents.tellp())
            Action(Contents.str());
    }

    template<typename T> TTestLogItem& operator <<(const T& value)
    {
        Contents << value;
        return *this;
    }

private:
    friend class TLoggedFixture;
    TTestLogItem(const TAction& action): Action(action) {}
    TAction Action;
    std::stringstream Contents;
};

class TLoggedFixture: public ::testing::Test
{
public:
    virtual ~TLoggedFixture();
    TTestLogItem Emit()
    {
        return TTestLogItem([this](const std::string& s) {
                Contents << s << std::endl;
            });
    }
    TTestLogItem Note()
    {
        return TTestLogItem([this](const std::string& s) {
                Contents << ">>> "  << s << std::endl;
            });
    }

    void TearDown();
private:
    bool IsOk();
    std::string GetFileName(const std::string& suffix = "") const;
    std::stringstream Contents;
};
