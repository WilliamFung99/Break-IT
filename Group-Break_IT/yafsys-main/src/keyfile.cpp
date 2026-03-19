#include "keyfile.hpp"
#include <fstream>
#include <random>
#include <filesystem>

namespace fs_impl = std::filesystem;

namespace keyfile {

static std::string randomToken() {
    static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    std::string s;
    for (int i = 0; i < 32; ++i) s += charset[dis(gen)];
    return s;
}

bool createUserKeyfile(const std::string& username,
                      const std::string& keyfile_path,
                      std::string& out_token) {
    out_token = randomToken();
    std::ofstream f(keyfile_path);
    if (!f) return false;
    f << username << "\n" << out_token;
    return f.good();
}

AuthResult authenticate(const std::string& keyfile_path,
                        const std::map<std::string, std::string>& token_registry) {
    AuthResult result{false, ""};
    std::ifstream f(keyfile_path);
    if (!f) return result;

    std::string username, token;
    if (!std::getline(f, username) || !std::getline(f, token)) return result;

    auto it = token_registry.find(token);
    if (it == token_registry.end() || it->second != username) return result;

    result.success = true;
    result.username = username;
    return result;
}

}
