#pragma once
#include <string>

namespace Config {
constexpr const char* defaults       = "Defaults";
constexpr const char* saveAcls       = "SaveAcls";
constexpr const char* savePremission = "SavePremission";

constexpr const char* includes = "Includes";
constexpr const char* excludes = "Excludes";
constexpr const char* acls     = "Acls";
constexpr const char* perms    = "Permission";
};    // namespace Config

class DotFile {
public:
    void add(std::string const& fileName, bool isRegex = false);
    void remove(std::string const& fileName);
    void init();
    // 根据配置文件同步
    void sync();
    // 还原
    void restore();

protected:
    std::string getAcls(std::string const& fileName);
    std::string getPermission(std::string const& fileName);
    std::string getFilePath(std::string const& fileName);
public:
    std::string getStdOut(std::string const& cmd);
};
