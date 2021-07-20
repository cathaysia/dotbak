#include <gtest/gtest.h>
#include <Dotfile.h>
#include <string.h>

int main(int argc, char** argv){
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(RESTORE, PRIVILAGE) {
    struct TestDotFile : public DotFile {
        const std::string PubCalcPerms(const std::string& perms) {
            return this->calcPerms(perms);
        }
    } testDot;
    std::string str = "user::rwx;group::r--;other:rwx";
    ASSERT_STREQ(testDot.PubCalcPerms("user::rwx;group::r--;other::rwx").c_str(), "747");
}
