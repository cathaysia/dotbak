#pragma once
#include <string>

namespace Config {
constexpr const char* defaults       = "Defaults";
constexpr const char* saveAcls       = "SaveAcls";

constexpr const char* includes = "Includes";
constexpr const char* excludes = "Excludes";
constexpr const char* acls     = "Acls";
};    // namespace Config

class DotFile {
public:
    void add(std::string const& fileName, bool isRegex = false);
    void exclude(std::string const& fileName);
    void init();
    // 根据配置文件同步
    void sync();
    // 还原
    void restore();

protected:
    std::string getAcls(std::string const& fileName);
    std::string getFilePath(std::string const& fileName);
    std::string runBash(std::string const& cmd);
    // 将 acl 列表转换为数字形式
    std::string calcPerms(std::string const& perms);
};
