#include <gtest/gtest.h>
#include <Dotfile.h>
#include <string.h>

int main(int argc, char** argv){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(RESTORE, PRIVILAGE) {
    struct TestDotFile : public DotFile {
        using DotFile::calcPerms;
    } testDot;
    std::string str = "user::rwx;group::r--;other:rwx";
    ASSERT_STREQ(testDot.calcPerms("user::rwx;group::r--;other::rwx").c_str(), "747");
}
