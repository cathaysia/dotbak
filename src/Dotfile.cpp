#include "Dotfile.h"

#include <filesystem>
#include <regex>
#include <stack>
#include <utility>
#include <vector>
#include <deque>

#include <fmt/format.h>
#include <inicpp.h>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>

#include <appversion.h>

namespace fs = std::filesystem;
namespace bp = boost::process;

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
    iniFile[Config::defaults][Config::saveAcls]       = true;
    iniFile[Config::defaults][Config::savePremission] = true;
    // includes 是由分号分割的文件列表
    iniFile[Config::defaults][Config::includes] = "";
    iniFile[Config::defaults][Config::excludes] = "";
    iniFile.save(filePath);
}

void DotFile::add(std::string const& fileName, bool isRegex) {
    std::string filePath = getFilePath(fileName);
    spdlog::debug("添加文件 {}", filePath);

    auto configPath = fmt::format("/etc/{0}/{0}.ini", PROGRAME_NAME);
    if(!fs::exists(configPath)) this->init();
    ini::IniFile iniFile;
    iniFile.load(configPath);
    // 检查文件是否已经在列表中了
    auto oldString = iniFile[Config::defaults][Config::includes].as<std::string>();
    if(oldString.find(filePath) != std::string::npos) return;
    // 查看文件是否在排除列表
    auto needs = iniFile[Config::defaults][Config::excludes].as<std::string>();
    boost::erase_all(needs, filePath + ";");
    iniFile[Config::defaults][Config::excludes] = needs;

    iniFile[Config::defaults][Config::includes] = fmt::format("{0}{1};", oldString, filePath);
    // 查看文件权限
    iniFile[filePath][Config::acls]  = getAcls(filePath);
    iniFile[filePath][Config::perms] = getPermission(filePath);
    // 如果是文件夹，则向下查看所有与文件夹权限不一样的文件
    fs::path path(filePath);
    if(fs::is_directory(path)) {
        spdlog::debug("对路径进行递归查看");
        std::stack<fs::path> dirList;
        dirList.push(path);
        for(auto& file: fs::directory_iterator(dirList.top())) {
            dirList.pop();
            if(fs::is_directory(file)) dirList.push(file);

            auto acls  = getAcls(file.path());
            auto perms = getPermission(file.path());
            if(acls != iniFile[filePath][Config::acls].as<std::string>()) {
                iniFile[file.path()][Config::acls] = acls;
            }
            if(perms != iniFile[filePath][Config::perms].as<std::string>()) {
                iniFile[file.path()][Config::perms] = perms;
            }
        }
    }

    iniFile.save(configPath);
    spdlog::debug("现在包含列表为 {}", iniFile[Config::defaults][Config::includes].as<std::string>());
}
void DotFile::remove(const std::string& fileName) {
    std::string filePath = getFilePath(fileName);
    spdlog::debug("排除文件 {}", filePath);

    auto configPath = fmt::format("/etc/{0}/{0}.ini", PROGRAME_NAME);
    if(!fs::exists(configPath)) {
        spdlog::debug("检查配置文件失败 {}", configPath);
        return;
    }
    // 此时配置文件必定存在
    ini::IniFile iniFile;
    iniFile.load(configPath);
    // 检查文件是否已经在列表中了
    auto oldString = iniFile[Config::defaults][Config::excludes].as<std::string>();
    if(oldString.find(filePath) != -1) return;
    iniFile[Config::defaults][Config::excludes] = fmt::format("{0}{1};", oldString, filePath);
    // 从 includes 中删除
    auto needs = iniFile[Config::defaults][Config::includes].as<std::string>();
    boost::erase_all(needs, filePath + ";");
    iniFile[Config::defaults][Config::includes] = needs;

    iniFile.save(configPath);
    spdlog::debug("现在包含列表为 {}", iniFile[Config::defaults][Config::includes].as<std::string>());
}

std::string DotFile::getAcls(std::string const& fileName) {
    return getStdOut(fmt::format(R"RRR(getfacl {} | egrep -v "(file:)|(flags:)" | tr "\n" ";" | sed 's/;;/\n/g' | sed 's/# //g')RRR", fileName));
}
std::string DotFile::getPermission(std::string const& fileName) {
    return getStdOut(fmt::format(R"RRR(ls -al {} | awk '{{print $1}}' | sed 's/.//' | sed 's/.$//')RRR", fileName));
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
    boost::split(includes,iniFile[Config::defaults][Config::includes].as<std::string>(), boost::is_any_of(";"));
    std::vector<std::string> excludes;
    boost::split(excludes, iniFile[Config::defaults][Config::excludes].as<std::string>(), boost::is_any_of(";"));
    // 对路径进行遍历
    while(includes.size()) {
        fs::path topPath = includes.at(0);
        includes.pop_front();
        bool scan = true;
        std::for_each(excludes.begin(), excludes.end(), [&](std::string const& item) {
            if(item == topPath) scan = false;
        });
        if(!scan) continue;
        // 如果是文件则直接进行同步操作
        if(!fs::is_directory(topPath)) {
            fs::copy(topPath, fmt::format("/etc/{}/backup", PROGRAME_NAME) + topPath.string());
            continue;
        }
        // 如果是路径则列出当前路径的文件并添加
        for(auto& file : fs::directory_iterator(topPath)) {
            includes.push_back(file.path());
        }
    }
}
void DotFile::restore() {
    // 根据 /etc/dotbak/dotbak.ini 将文件恢复到原路径，并恢复文件权限
}

/***
 * cmd 中的大括号需要双写
 */
std::string DotFile::getStdOut(const std::string& cmd) {
    bp::ipstream is;
    auto cmdString = fmt::format(R"RRR(bash -c "echo `env LANG=en_US.UTF-8 {}`")RRR", cmd);
    spdlog::debug(fmt::format("执行的命令是： {}", cmdString));
    // 使用英文环境
    bp::child                c(cmdString, bp::std_out > is);
    std::vector<std::string> data;
    std::string              line;

    while(c.running() && std::getline(is, line) && !line.empty()) data.push_back(line);
    return boost::join(data, "\n");
}
