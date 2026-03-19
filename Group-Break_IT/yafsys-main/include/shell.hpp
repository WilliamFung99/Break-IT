#ifndef SHELL_HPP
#define SHELL_HPP

#include <string>
#include "filesystem.hpp"

namespace shell {
// Shell provides a command-line interface for interacting with
// the VirtualFileSystem including file operations, sharing,
// directory navigation, and administrative commands.
class Shell {
public:
    Shell(fs::VirtualFileSystem& fs,
          const std::string& username,
          bool is_admin);

    void run();

private:
    fs::VirtualFileSystem& fs_;
    std::string username_;
    bool is_admin_;
    std::string cwd_;

    void processCommand(const std::string& line);
    void cmdCd(const std::vector<std::string>& args);
    void cmdPwd(const std::vector<std::string>& args);
    void cmdLs(const std::vector<std::string>& args);
    void cmdCat(const std::vector<std::string>& args);
    void cmdShare(const std::vector<std::string>& args);
    void cmdMkdir(const std::vector<std::string>& args);
    void cmdMkfile(const std::vector<std::string>& args);
    void cmdExit(const std::vector<std::string>& args);
    void cmdAdduser(const std::vector<std::string>& args);
    // Writes content to a specified file in the virtual file system.
    // Expected usage: write <filename> <content>
    void cmdWrite(const std::vector<std::string>& args);
    // Splits a command line string into tokens separated by whitespace
    std::vector<std::string> tokenize(const std::string& line);
};

}
#endif
