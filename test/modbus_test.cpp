#include <map>
#include <memory>
#include <cassert>
#include <gtest/gtest.h>

#include "testlog.h"

class SampleFixture: public TLoggedFixture
{ 
   LOGGED_FIXTURE(SampleFixture)
};

TEST_F(SampleFixture, SampleTest)
{
    ASSERT_TRUE(1);
    Note() << "ugu";
    Emit() << "aga!";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
