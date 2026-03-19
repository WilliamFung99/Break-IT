#ifndef KEYFILE_HPP
#define KEYFILE_HPP

#include <string>
#include <map>

namespace keyfile {

struct AuthResult {
    bool success;
    std::string username;
};

bool createUserKeyfile(const std::string& username,
                      const std::string& keyfile_path,
                      std::string& out_token);

AuthResult authenticate(const std::string& keyfile_path,
                        const std::map<std::string, std::string>& token_registry);

}
#endif
