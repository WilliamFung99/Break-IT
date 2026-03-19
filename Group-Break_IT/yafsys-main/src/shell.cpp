#include "shell.hpp"
#include "keyfile.hpp"
#include <iostream>
#include <sstream>

namespace shell {

Shell::Shell(fs::VirtualFileSystem& fs,
             const std::string& username,
             bool is_admin)
    : fs_(fs)
    , username_(username)
    , is_admin_(is_admin)
    , cwd_("/") {}

std::vector<std::string> Shell::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    bool in_quotes = false;
    char quote_char = 0;
    std::string current;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == quote_char && (i == 0 || line[i-1] != '\\')) {
                in_quotes = false;
                quote_char = 0;
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        } else if (c == '"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (c == ' ' || c == '\t') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void Shell::processCommand(const std::string& line) {
    auto tokens = tokenize(line);
    if (tokens.empty()) return;

    std::string cmd = tokens[0];
    if (cmd == "cd") cmdCd(tokens);
    else if (cmd == "pwd") cmdPwd(tokens);
    else if (cmd == "ls") cmdLs(tokens);
    else if (cmd == "cat") cmdCat(tokens);
    else if (cmd == "share") cmdShare(tokens);
    else if (cmd == "mkdir") cmdMkdir(tokens);
    else if (cmd == "mkfile") cmdMkfile(tokens);
    else if (cmd == "exit") cmdExit(tokens);
    else if (cmd == "write") cmdWrite(tokens);
    else if (cmd == "adduser" && is_admin_) cmdAdduser(tokens);
    else std::cout << "Invalid Command\n";
}

void Shell::cmdCd(const std::vector<std::string>& args) {
    if (args.size() < 2) return;

    std::string target = args[1];
    for (size_t i = 2; i < args.size(); ++i)
        target += " " + args[i];

    // Admin special navigation
    if (is_admin_ && cwd_ == "/" && fs_.userExists(target)) {
        username_ = target;
        cwd_ = "/";
        return;
    }

    // Return to admin view
    if (is_admin_ && target == "/") {
        username_ = "admin";
        cwd_ = "/";
        return;
    }

    std::string resolved = fs_.resolvePath(cwd_, target);
    if (!fs_.isValidPath(resolved) || fs_.pathEscapesRoot(resolved)) return;

    if (fs_.directoryExists(username_, resolved)) {
        cwd_ = resolved;
    }
}

void Shell::cmdPwd(const std::vector<std::string>& args) {
    (void)args;
    std::cout << cwd_ << "\n";
}

void Shell::cmdLs(const std::vector<std::string>& args) {
    (void)args;

    // Admin root view: show all users
    if (is_admin_ && cwd_ == "/") {
        std::cout << "d -> .\n";
        std::cout << "d -> ..\n";

        for (const auto& u : fs_.getAllUsers()) {
            std::cout << "d -> " << u << "\n";
        }
        return;
    }

    auto entries = fs_.listDirectory(username_, cwd_);
    for (const auto& [name, is_dir] : entries) {
        std::cout << (is_dir ? "d" : "f") << " -> " << name << "\n";
    }
}

void Shell::cmdCat(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Invalid Command\n";
        return;
    }
    std::string fname = args[1];
    std::string fpath = fs_.resolvePath(cwd_, fname);
    if (!fs_.isValidPath(fpath) || fs_.pathEscapesRoot(fpath)) {
        std::cout << fname << " doesn't exist\n";
        return;
    }
    auto content = fs_.readFile(username_, fpath);
    if (!content) {
        std::cout << fname << " doesn't exist\n";
        return;
    }
    std::cout << *content << "\n";
}

void Shell::cmdShare(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Invalid Command\n";
        return;
    }
    std::string fname = args[1];
    std::string target_user = args[2];
    std::string fpath = fs_.resolvePath(cwd_, fname);

    if (!fs_.fileExists(username_, fpath)) {
        std::cout << "File " << fname << " doesn't exist\n";
        return;
    }
    if (!fs_.userExists(target_user)) {
        std::cout << "User " << target_user << " doesn't exist\n";
        return;
    }
    if (!fs_.shareFile(username_, fpath, target_user)) {
        std::cout << "Invalid Command\n";
        return;
    }
    fs_.save();
}

void Shell::cmdMkdir(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Invalid Command\n";
        return;
    }
    std::string dname = args[1];
    std::string dpath = fs_.resolvePath(cwd_, dname);

    if (fs_.pathEscapesRoot(dpath) || fs_.isRootOrSharedRoot(dpath) ||
        fs_.isSharedPath(dpath) || !fs_.isValidPath(dpath)) {
        std::cout << "Forbidden\n";
        return;
    }
    if (fs_.directoryExists(username_, dpath)) {
        std::cout << "Directory already exists\n";
        return;
    }
    if (!fs_.createDirectory(username_, dpath)) {
        std::cout << "Directory already exists\n";
        return;
    }
    fs_.save();
}

void Shell::cmdMkfile(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Invalid Command\n";
        return;
    }

    std::string fname = args[1];

    // Optional content
    std::string content;
    if (args.size() > 2) {
        content = args[2];
        for (size_t i = 3; i < args.size(); ++i) {
            content += " " + args[i];
        }
    }

    std::string fpath = fs_.resolvePath(cwd_, fname);

    if (fs_.pathEscapesRoot(fpath) ||
        fs_.isRootOrSharedRoot(fpath) ||
        fs_.isSharedPath(fpath) ||
        !fs_.isValidPath(fpath)) {
        std::cout << "Forbidden\n";
        return;
    }

    if (!fs_.createFile(username_, fpath, content)) {
        std::cout << "Forbidden\n";
        return;
    }

    fs_.save();
}
// Handles the 'write' shell command.
// Usage: write <filename> <content>
// Writes the provided content to an existing file in the virtual filesystem.
void Shell::cmdWrite(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Invalid Command\n";
        return;
    }

    std::string fname = args[1];
    std::string content = args[2];

    for (size_t i = 3; i < args.size(); ++i) {
        content += " " + args[i];
    }

    std::string fpath = fs_.resolvePath(cwd_, fname);

    if (!fs_.writeFile(username_, fpath, content)) {
        std::cout << "File doesn't exist\n";
        return;
    }

    fs_.save();
}

void Shell::cmdExit(const std::vector<std::string>& args) {
    (void)args;
    exit(0);
}

void Shell::cmdAdduser(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Invalid Command\n";
        return;
    }
    std::string new_user = args[1];
    if (fs_.userExists(new_user)) {
        std::cout << "User " << new_user << " already exists\n";
        return;
    }

    std::string base = fs_.getBasePath();
    size_t pos = base.rfind('/');
    std::string parent = (pos != std::string::npos) ? base.substr(0, pos) : ".";
    std::string keyfile_path = parent + "/" + new_user + "_keyfile";
    std::string token;

    if (!keyfile::createUserKeyfile(new_user, keyfile_path, token)) {
        std::cout << "Invalid Command\n";
        return;
    }

    fs_.addTokenForUser(token, new_user);
    fs_.ensureUser(new_user);
    fs_.save();
}

void Shell::run() {
    std::string line;
    while (std::cout << "> ", std::getline(std::cin, line)) {
        processCommand(line);
    }
}

}
