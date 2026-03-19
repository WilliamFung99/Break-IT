#include "keyfile.hpp"
#include "filesystem.hpp"
#include "shell.hpp"
#include <iostream>
#include <filesystem>

namespace fs_impl = std::filesystem;

static bool keyfileInRoot(const std::string& keyfile_path, const std::string& root) {
    fs_impl::path kp(keyfile_path);
    fs_impl::path rp(root);
    try {
        auto rel = fs_impl::relative(kp.lexically_normal(), rp.lexically_normal());
        if (rel.empty() || *rel.begin() == "..") return false;
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " keyfile_name\n";
        return 1;
    }

    std::string cwd = fs_impl::current_path().string();
    fs_impl::path keyfile_arg(argv[1]);
    if (!keyfile_arg.is_absolute()) {
        keyfile_arg = fs_impl::current_path() / keyfile_arg;
    }
    std::string keyfile_path = keyfile_arg.lexically_normal().string();

    if (!keyfileInRoot(keyfile_path, cwd)) {
        std::cout << "Invalid keyfile\n";
        return 1;
    }

    std::string filesystem_dir = cwd + "/filesystem";
    std::string keys_dir = cwd;

    fs_impl::create_directories(filesystem_dir);

    fs::VirtualFileSystem vfs;

    std::string meta_path = filesystem_dir + "/metadata";
    bool first_run = !fs_impl::exists(meta_path);

    if (first_run) {
        std::string admin_keyfile = cwd + "/admin_keyfile";
        std::string token;

        if (!keyfile::createUserKeyfile("admin", admin_keyfile, token)) {
            std::cerr << "Failed to create admin keyfile\n";
            return 1;
        }

        vfs.initialize(filesystem_dir);
        vfs.addTokenForUser(token, "admin");
        vfs.ensureUser("admin");
        vfs.save();
    } else {
        if (!vfs.load(filesystem_dir)) {
            vfs.initialize(filesystem_dir);
        }
    }

    auto auth = keyfile::authenticate(keyfile_path, vfs.getTokenRegistry());
    if (!auth.success) {
        std::cout << "Invalid keyfile\n";
        return 1;
    }

    std::string username = auth.username;
    std::cout << "Logged in as " << username << "\n";

    if (first_run && username != "admin") {
        vfs.initialize(filesystem_dir);
        vfs.ensureUser(username);
    } else if (!first_run && !vfs.userExists(username)) {
        std::cout << "Invalid keyfile\n";
        return 1;
    }

    bool is_admin = (username == "admin");
    shell::Shell sh(vfs, username, is_admin);
    sh.run();

    return 0;
}
