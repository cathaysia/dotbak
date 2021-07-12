#include <filesystem>
#include <iostream>
#include <regex>
#include <tuple>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <boost/program_options.hpp>

#include "Dotfile.h"

#include <appversion.h>

namespace po = boost::program_options;
namespace fs = std::filesystem;

typedef std::tuple<po::options_description, po::variables_map, std::vector<std::string>> ShareData;

ShareData set_program_options(int argc, char** argv);

int main(int argc, char** argv) {
    spdlog::set_pattern("%^%l%$: %v");
    spdlog::set_level(spdlog::level::debug);
    // 设置命令行选项
    auto  shareData = set_program_options(argc, argv);
    auto& vm        = std::get<1>(shareData);
    if(vm.count("help") || (argc == 1)) {
        std::cout << std::get<0>(shareData) << std::endl;
        exit(EXIT_SUCCESS);
    }
    // add 和 exclude 不能共存
    if(vm.count("add") && vm.count("exclude")) {
        spdlog::error(_("add 和 exclude 选项不能共存"));
        exit(EXIT_SUCCESS);
    }
    DotFile dotFile;
    if(vm.count("add")) {
        dotFile.add(vm["add"].as<std::string>());
        for(auto& file: std::get<2>(shareData)) { dotFile.add(file, vm.count("regex")); }
    }
    if(vm.count("exclude")) {
        dotFile.remove(vm["exclude"].as<std::string>());
        for(auto& file: std::get<2>(shareData)) { dotFile.remove(file); }
    }
    spdlog::debug("检查 backup 选项");
    if(vm.count("backup")) {
        dotFile.sync();
        exit(EXIT_SUCCESS);
    }

    return 0;
}

ShareData set_program_options(int argc, char** argv) {
    po::options_description desc(PROGRAME_DESC);
    // clang-format off
    desc.add_options()
        ("help,h", _("Print usage"))
        ("add,a", po::value<std::string>(),_("add a file or directory to backup list"))
        // 此选项仅在 --add 选项指定时才有意义
        /**
         *@todo
         * 使用正则表达式匹配文件
         */
//         ("regex,r"  ,po::value<std::string>(),_("--add with regex support"))
        // 从同步列表中排除一个文件
        ("exclude,e", po::value<std::string>(), _("exclude a file from sync list"))
        ("backup,b", po::value<bool>()->default_value(false), _("Backup file"))
    ;
    //clang-format on
    po::variables_map vm;
    auto parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
    po::store(parsed, vm);
    po::notify(vm);

    auto unregistered = po::collect_unrecognized(parsed.options,
po::include_positional);
    for(auto &unknow : unregistered){
    spdlog::debug("未知选项为： {}", unknow);

    }
    return ShareData(std::move(desc), std::move(vm), std::move(unregistered));
}
