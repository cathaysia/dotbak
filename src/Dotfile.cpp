#include "Dotfile.h"

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/exception/exception.hpp>
#include <boost/process/system.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <regex>
#include <stack>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>
#include <inicpp.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>

#include <appversion.h>

namespace fs = std::filesystem;
namespace bp = boost::process;

inline int permToI(const char p);

void DotFile::init() {
    // 配置文件位于 /etc/PROGRAME_NAME/PROGRAME_NAME.json
    // 检查是否存在配置文件
    // 先检查路径是否存在
    auto filePath = fmt::format("/etc/{0}/", PROGRAME_NAME);
    if(!fs::exists(filePath)) {
        spdlog::info(fmt::format("文件夹 {} 不存在，重新创建", filePath));
        fs::create_directory(filePath);
    }
    // 初始化配置
    filePath += PROGRAME_NAME ".ini";
    if(fs::exists(filePath)) return;
    ini::IniFile iniFile;
    iniFile[Config::defaults][Config::saveAcls] = true;
    iniFile[Config::defaults][Config::includes] = "";
    iniFile[Config::defaults][Config::excludes] = "";
    iniFile.save(filePath);
}

void DotFile::add(std::string const& fileName, bool isRegex) {
    auto configPath = fmt::format("/etc/{0}/{0}.ini", PROGRAME_NAME);
    if(!fs::exists(configPath)) this->init();
    ini::IniFile iniFile;
    iniFile.load(configPath);

    std::string filePath = getFilePath(fileName);
    spdlog::debug("添加文件 {}", filePath);
    // 将文件从排除列表中删除
    auto needs = iniFile[Config::defaults][Config::excludes].as<std::string>();
    boost::erase_all(needs, filePath + ";");
    iniFile[Config::defaults][Config::excludes] = needs;

    // 检查文件是否已经在列表中了
    auto oldString = iniFile[Config::defaults][Config::includes].as<std::string>();
    if(oldString.find(filePath) != std::string::npos) return;
    iniFile[Config::defaults][Config::includes] = fmt::format("{0}{1};", oldString, filePath);

    // 查看文件权限
    auto mainAcls                   = getAcls(filePath);
    iniFile[filePath][Config::acls] = mainAcls;
    // 如果是文件夹，则向下查看所有与文件夹权限不一样的文件
    fs::path path(filePath);
    if(fs::is_directory(path)) {
        spdlog::debug("对路径 {} 进行递归查看", path.string());
        std::stack<fs::path> dirList;
        dirList.push(path);
        for(auto& file: dirList.top()) {
            dirList.pop();
            if(fs::is_directory(fs::path(file))) dirList.push(file);
            auto acls = getAcls(file);
            if(acls != mainAcls) iniFile[file][Config::acls] = acls;
        }
    }

    iniFile.save(configPath);
    spdlog::debug("现在包含列表为 {}", iniFile[Config::defaults][Config::includes].as<std::string>());
}
void DotFile::exclude(const std::string& fileName) {
    auto configPath = fmt::format("/etc/{0}/{0}.ini", PROGRAME_NAME);
    if(!fs::exists(configPath)) this->init();
    ini::IniFile iniFile;
    iniFile.load(configPath);

    std::string filePath = getFilePath(fileName);
    spdlog::debug("排除文件 {}", filePath);
    // 检查文件是否已经在列表中了
    auto oldString = iniFile[Config::defaults][Config::excludes].as<std::string>();
    if(oldString.find(filePath) != std::string::npos) return;
    iniFile[Config::defaults][Config::excludes] = fmt::format("{0}{1};", oldString, filePath);
    // 从 includes 中删除
    auto needs = iniFile[Config::defaults][Config::includes].as<std::string>();
    boost::erase_all(needs, filePath + ";");
    iniFile[Config::defaults][Config::includes] = needs;

    iniFile.save(configPath);
    spdlog::debug("现在包含列表为 {}", iniFile[Config::defaults][Config::includes].as<std::string>());
}

/**
  权限为：
# file: 1
# owner: tea
# group: tea
# flags: sst
user::rwx
user:root:rwx
group::r--
group:docker:rwx
mask::rwx
other::rwx
输出为：
flags: sst;user::rwx;user:root:rwx;group::r--;group:docker:rwx;other::rwx
数字权限为：
7777
*/
std::string DotFile::getAcls(std::string const& fileName) {
    return runBash(fmt::format(
        R"RRR(getfacl -p {} | egrep -v "(file:)|(owner: )|(group: )|(mask)" | tr "\n" ";" | sed "s/;;/\n/g" | sed "s/# //g")RRR",
        fileName));
}

std::string DotFile::getFilePath(std::string const& fileName) {
    fs::path path(fileName);
    if(path.is_relative()) path = std::filesystem::current_path().string() + "/" + fileName;
    if(!fs::exists(path)) {
        spdlog::error("文件不存在 {}", path.string());
        exit(EXIT_FAILURE);
    }
    return path.string();
}

void DotFile::sync() {
    // 根据 defaults/includes 和 defaults/excludes 将文件从原路径同步到 /etc/dotbak/bakup
    // .git 路径会被压缩
    auto configPath = fmt::format("/etc/{0}/{0}.ini", PROGRAME_NAME);
    if(!fs::exists(configPath)) return;
    ini::IniFile iniFile;
    iniFile.load(configPath);

    std::deque<std::string> includes;
    boost::split(includes, iniFile[Config::defaults][Config::includes].as<std::string>(), boost::is_any_of(";"));
    std::vector<std::string> excludes;
    boost::split(excludes, iniFile[Config::defaults][Config::excludes].as<std::string>(), boost::is_any_of(";"));
    // 理论上来讲，路径越短，越是顶级路径
    // 这里要从顶级路径开始查看
    std::sort(includes.begin(), includes.end(), [](std::string const& a, std::string const& b) {
        return a.length() < b.length();
    });
    // 对路径进行遍历
    while(includes.size()) {
        fs::path topPath = includes.at(0);
        includes.pop_front();
        if(!fs::exists(topPath)) continue;
        // 如果是文件则直接进行同步操作
        if(!fs::is_directory(topPath)) {
            auto des = fs::path(fmt::format("/etc/{}/backup", PROGRAME_NAME) + topPath.string());
            if(!fs::exists(des.remove_filename())) runBash(fmt::format(R"RRR(mkdir -p {})RRR", des.string()));
            spdlog::debug("拷贝文件 {} 到 {}", topPath.string(), des.string());
            auto cpOut = runBash(fmt::format(R"RRR(cp {} {})RRR", topPath.string(), des.string()));
            if(cpOut.length()) spdlog::info(cpOut);
            continue;
        }
        // 如果是路径，就先创建路径，再对路径下的文件进行扫描
        runBash(fmt::format("mkdir -p /etc/{}/backup{}", PROGRAME_NAME, topPath.string()));

        for(auto& file: fs::directory_iterator(topPath)) {
            // 检查文件是否在排除列表中
            bool addFile = true;
            std::for_each(excludes.begin(), excludes.end(), [&](std::string const& item) {
                if(item == topPath) addFile = false;
            });
            if(!addFile) continue;

            includes.push_back(file.path());
        }
    }
}
void DotFile::restore() {
    // 根据 /etc/dotbak/dotbak.ini 将文件恢复到原路径，并恢复文件权限
    runBash(fmt::format("cp -r /etc/{0}/backup/* /", PROGRAME_NAME));
    // 恢复文件权限
    auto configPath = fmt::format("/etc/{0}/{0}.ini", PROGRAME_NAME);
    if(!fs::exists(configPath)) return;
    ini::IniFile iniFile;
    iniFile.load(configPath);
    // 获取需要恢复权限的文件的列表
    auto fileListString =
        runBash(fmt::format(R"RRR(cat {} | grep '\['| sed 's/\[//g' | sed 's/\]//g')RRR", configPath));
    std::vector<std::string> fileList;
    boost::split(fileList, fileListString, boost::is_any_of(" "));
    for(auto it = fileList.begin(); it!=fileList.end(); ++it){
        if(*it == Config::defaults) fileList.erase(it);
    }
    spdlog::debug("需要恢复权限的文件列表为： [{}]", boost::join(fileList, ";"));

    for(const auto& file: fileList) {
        if(!iniFile[file][Config::acls].as<std::string>().length()) continue;
        // 清空权限
        runBash(fmt::format(R"RRR(setfacl -k {})RRR", file));
        runBash(fmt::format(R"RRR(chmod 000 {})RRR", file));
        /**
         * todo
         * 将文件权限解析为数字形式，并且只使用 acl 权限
         */
        auto acls     = iniFile[file][Config::acls].as<std::string>();
        spdlog::debug("{}: 得到的 acl 权限为： {}", __FUNCTION__, acls);
        auto mainPerm = calcPerms(acls);
        runBash(fmt::format("chmod {0} {1}", mainPerm, file));
        spdlog::debug("将文件 {} 的权限设置为 {}", file, mainPerm);
        // 将已经获取的权限删除
        auto rmPerms = [&acls](const char* str) {
            size_t pos = std::string::npos;
            if((pos = acls.find(str)) != std::string ::npos) acls = acls.replace(pos, strlen(str) + 4, "");
        };
        rmPerms("flags: ");
        rmPerms("user::");
        rmPerms("group::");
        rmPerms("other::");
        spdlog::debug("清理后的权限为： {}", acls);
        // 其他用户的权限
        std::vector<std::string> oPerms;
        boost::split(oPerms, acls, boost::is_any_of(";"));
        spdlog::debug("其他用户的权限为： {}", boost::join(oPerms, ";"));
        spdlog::debug("oPerms.size() == {}", oPerms.size());
        for(auto const& p: oPerms) {
            std::vector<std::string> part;
            boost::split(part, p, boost::is_any_of(":"));
            if(part.size() != 3) continue;
            char type = 'u';
            if(part.at(0) == "group") type = 'g';
            runBash(fmt::format("getfacl '{}:{}:{}' {}", type, part.at(1), part.at(2), file));
        }
    }
}

/***
 * cmd 中的大括号需要双写
 */
std::string DotFile::runBash(const std::string& cmd) {
    //    bp::ipstream is;
    //    auto         cmdString = fmt::format(R"RRR(bash -c 'echo `env LANG=en_US.UTF-8 {}`')RRR", cmd);
    //    spdlog::debug(fmt::format("执行的命令是： {}", cmdString));
    //    // 使用英文环境
    //    bp::child                c(cmdString, bp::std_out > is);
    //    std::vector<std::string> data;
    //    std::string              line;
    //    while(c.running() && std::getline(is, line) && !line.empty()) data.push_back(line);
    //    auto result = boost::join(data, "\n");
    //    spdlog::debug("命令的结果为： {}", result);
    //    return result;
    auto cmdString = fmt::format(R"RRR(echo `env LANG=en_US.UTF-8 {}`)RRR", cmd);
    spdlog::debug(fmt::format("执行的命令是： {}", cmdString));
    FILE* fp = nullptr;
    if((fp = popen(cmdString.c_str(), "r")) == nullptr) { throw std::runtime_error("命令执行错误"); }
    char                     line[300];
    std::vector<std::string> data;
    while(fgets(line, sizeof(line) - 1, fp)) { data.push_back(line); }
    pclose(fp);
    auto result = boost::join(data, "\n");
    spdlog::debug("命令的结果为： {}", result);
    return result;
}

std::string DotFile::calcPerms(std::string const& perms) {
    /* 权限的数字形式一共有四种，除了传统的 rwx 生成的 777 之外，
     * 在第零位再插入一个数字，分别代表：
     * 1: SBIT t
     * 2: SGID s
     * 4: SUID s
     * 如果文件没有 x 权限却被赋予了特殊权限，则形式为大写
     * 特殊权限只有 owner 才有，其他的没有
     * 此函数要求 perm 包含 user::rwx;group::r--;other:rwx
     */
    std::string mainPerm;
    int         permInt = 0;
    size_t      pos     = std::string::npos;
    if((pos = perms.find("flags")) != std::string::npos) {
        auto perm = perms.substr(pos, 10);
        boost::replace_all(perm, "flags: ", "");
        if(perm[0] == 's') permInt |= 4;
        if(perm[1] == 's') permInt |= 2;
        if(perm[2] == 't') permInt |= 1;
        mainPerm += fmt::format("{}", permInt);
    }
    spdlog::debug("flags/mainPerm = {}", mainPerm);
    // 检查以下权限：
    // user::rwx
    permInt   = 0;
    auto uPos = perms.find("user::");
    assert(uPos != std::string::npos);
    uPos += 5;
    permInt |= permToI(perms[++uPos]);
    permInt |= permToI(perms[++uPos]);
    permInt |= permToI(perms[++uPos]);
    mainPerm += fmt::format("{}", permInt);
    spdlog::debug("user::/mainPerm = {}", mainPerm);
    // group::r--
    permInt   = 0;
    auto gPos = perms.find("group::");
    assert(gPos != std::string::npos);
    gPos += 6;
    permInt |= permToI(perms[++gPos]);
    permInt |= permToI(perms[++gPos]);
    permInt |= permToI(perms[++gPos]);
    mainPerm += fmt::format("{}", permInt);
    spdlog::debug("group/mainPerm = {}", mainPerm);
    // other::rwx
    permInt   = 0;
    auto oPos = perms.find("other::");
    assert(oPos != std::string::npos);
    oPos += 6;
    permInt |= permToI(perms[++oPos]);
    permInt |= permToI(perms[++oPos]);
    permInt |= permToI(perms[++oPos]);
    mainPerm += fmt::format("{}", permInt);

    spdlog::debug("other/mainPerm = {}", mainPerm);

    // 返回形式为 1770; 770
    return mainPerm;
}

int permToI(const char p) {
    if(p == 'r') return 4;
    if(p == 'w') return 2;
    if(p == 'x') return 1;
    if(p == '-') return 0;
    throw std::runtime_error(fmt::format("出现了错误权限： {}", p));
}
