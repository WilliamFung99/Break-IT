#include "filesystem.hpp"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <filesystem>

namespace fs_impl = std::filesystem;

namespace fs {

static std::string trimSlashes(const std::string& s) {
    size_t start = s.find_first_not_of('/');
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of('/');
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::string p = trimSlashes(path);
    if (p.empty()) return parts;

    std::stringstream ss(p);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) parts.push_back(part);
    }
    return parts;
}

void VirtualFileSystem::ensureUser(const std::string& user) {
    if (user_personal_.find(user) == user_personal_.end()) {
        user_personal_[user] = DirEntry{};
        user_shared_[user] = DirEntry{};
    }
}

void VirtualFileSystem::addTokenForUser(const std::string& token, const std::string& username) {
    token_registry_[token] = username;
}

bool VirtualFileSystem::initialize(const std::string& base_path) {
    base_path_ = base_path;
    fs_impl::create_directories(base_path);
    ensureUser("admin");
    return true;
}

std::string VirtualFileSystem::normalizePath(const std::string& path) const {
    std::string p = path;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    if (p.empty()) p = "/";
    if (p[0] != '/') p = "/" + p;
    return p;
}

std::string VirtualFileSystem::resolvePath(const std::string& current,
                                          const std::string& target) const {
    std::string base = current;
    if (base.empty() || base[0] != '/') base = "/" + base;
    if (base.size() > 1 && base.back() == '/') base.pop_back();

    std::string t = target;
    if (t.empty()) return base;

    if (t[0] == '/') return normalizePath(t);

    std::vector<std::string> parts = splitPath(base);
    std::vector<std::string> add = splitPath(t);

    for (const auto& part : add) {
        if (part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(part);
        }
    }

    if (parts.empty()) return "/";
    std::string result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "/";
        result += parts[i];
    }
    return result;
}

bool VirtualFileSystem::pathEscapesRoot(const std::string& path) const {
    std::string p = normalizePath(path);
    if (p == "/") return false;
    auto parts = splitPath(p);
    if (parts.empty()) return false;
    return parts[0] != "personal" && parts[0] != "shared";
}

bool VirtualFileSystem::isValidPath(const std::string& path) const {
    if (pathEscapesRoot(path)) return false;
    std::string p = normalizePath(path);
    if (p == "/") return true;
    auto parts = splitPath(p);
    if (parts.empty()) return true;
    return parts[0] == "personal" || parts[0] == "shared";
}

bool VirtualFileSystem::isSharedPath(const std::string& path) const {
    auto parts = splitPath(path);
    return !parts.empty() && parts[0] == "shared";
}

bool VirtualFileSystem::isRootOrSharedRoot(const std::string& path) const {
    std::string p = normalizePath(path);
    if (p == "/") return true;
    auto parts = splitPath(p);
    return parts.size() == 1 && parts[0] == "shared";
}

bool VirtualFileSystem::directoryExists(const std::string& user,
                                        const std::string& path) const {
    if (pathEscapesRoot(path)) return false;
    std::string p = normalizePath(path);
    if (p == "/") return true;

    auto parts = splitPath(p);
    if (parts.empty()) return true;
    if (parts[0] != "personal" && parts[0] != "shared") return false;

    bool use_shared = (parts[0] == "shared");
    const DirEntry* dir = use_shared ? &user_shared_.at(user) : &user_personal_.at(user);

    for (size_t i = 1; i < parts.size(); ++i) {
        auto it = dir->subdirs.find(parts[i]);
        if (it == dir->subdirs.end()) return false;
        if (use_shared) {
            dir = &user_shared_.at(it->second);
        } else {
            dir = &user_personal_.at(it->second);
        }
    }
    return true;
}

bool VirtualFileSystem::fileExists(const std::string& user,
                                   const std::string& path) const {
    if (pathEscapesRoot(path)) return false;
    std::string p = normalizePath(path);
    auto parts = splitPath(p);
    if (parts.size() < 2) return false;
    if (parts[0] != "personal" && parts[0] != "shared") return false;

    std::string parentPath = p.substr(0, p.find_last_of('/'));
    if (parentPath.empty()) parentPath = "/";
    const DirEntry* dir = getDirAt(user, parentPath, parts[0] == "shared");
    if (!dir) return false;

    return dir->files.find(parts.back()) != dir->files.end();
}

std::vector<std::pair<std::string, bool>> VirtualFileSystem::listDirectory(
    const std::string& user, const std::string& path) const {
    std::vector<std::pair<std::string, bool>> result;
    result.emplace_back(".", true);
    result.emplace_back("..", true);

    if (pathEscapesRoot(path)) return result;

    std::string p = normalizePath(path);
    if (p == "/") {
        result.emplace_back("personal", true);
        result.emplace_back("shared", true);
        return result;
    }

    const DirEntry* dir = getDirAt(user, p, false);
    if (!dir) dir = getDirAt(user, p, true);
    if (!dir) return result;

    for (const auto& [name, _] : dir->subdirs) {
        result.emplace_back(name, true);
    }
    for (const auto& [name, _] : dir->files) {
        result.emplace_back(name, false);
    }
    return result;
}

const DirEntry* VirtualFileSystem::getDirAt(const std::string& user,
                                            const std::string& path,
                                            bool prefer_shared) const {
    auto it_p = user_personal_.find(user);
    auto it_s = user_shared_.find(user);
    if (it_p == user_personal_.end() || it_s == user_shared_.end()) return nullptr;

    std::string p = normalizePath(path);
    auto parts = splitPath(p);

    if (parts.empty()) return nullptr;
    if (parts[0] == "personal") {
        const DirEntry* dir = &it_p->second;
        for (size_t i = 1; i < parts.size(); ++i) {
            auto sub = dir->subdirs.find(parts[i]);
            if (sub == dir->subdirs.end()) return nullptr;
            dir = &user_personal_.at(sub->second);
        }
        return dir;
    }
    if (parts[0] == "shared") {
        const DirEntry* dir = &it_s->second;
        for (size_t i = 1; i < parts.size(); ++i) {
            auto sub = dir->subdirs.find(parts[i]);
            if (sub == dir->subdirs.end()) return nullptr;
            dir = &user_shared_.at(sub->second);
        }
        return dir;
    }
    return nullptr;
}

DirEntry* VirtualFileSystem::getMutableDirAt(const std::string& user,
                                             const std::string& path,
                                             bool shared_only) {
    auto it_p = user_personal_.find(user);
    auto it_s = user_shared_.find(user);
    if (it_p == user_personal_.end() || it_s == user_shared_.end()) return nullptr;

    std::string p = normalizePath(path);
    auto parts = splitPath(p);

    if (parts.empty()) return nullptr;
    if (parts[0] == "shared") {
        DirEntry* dir = &user_shared_[user];
        for (size_t i = 1; i < parts.size(); ++i) {
            auto sub = dir->subdirs.find(parts[i]);
            if (sub == dir->subdirs.end()) return nullptr;
            dir = &user_shared_[sub->second];
        }
        return dir;
    }
    if (parts[0] == "personal" && !shared_only) {
        DirEntry* dir = &user_personal_[user];
        for (size_t i = 1; i < parts.size(); ++i) {
            auto sub = dir->subdirs.find(parts[i]);
            if (sub == dir->subdirs.end()) return nullptr;
            dir = &user_personal_[sub->second];
        }
        return dir;
    }
    return nullptr;
}

DirEntry& VirtualFileSystem::getOrCreatePersonal(const std::string& user) {
    ensureUser(user);
    return user_personal_[user];
}

bool VirtualFileSystem::createDirectory(const std::string& user,
                                        const std::string& path) {
    if (pathEscapesRoot(path)) return false;
    std::string p = normalizePath(path);
    if (isRootOrSharedRoot(p)) return false;

    auto parts = splitPath(p);
    if (parts.size() < 2) return false;
    if (parts[0] != "personal") return false;

    ensureUser(user);
    DirEntry* dir = &user_personal_[user];
    for (size_t i = 1; i < parts.size() - 1; ++i) {
        auto it = dir->subdirs.find(parts[i]);
        if (it == dir->subdirs.end()) return false;
        dir = &user_personal_[it->second];
    }

    std::string name = parts.back();
    if (dir->subdirs.count(name) || dir->files.count(name)) return false;

    std::string new_user = user + ":" + p;
    user_personal_[new_user] = DirEntry{};
    dir->subdirs[name] = new_user;
    return true;
}

bool VirtualFileSystem::createFile(const std::string& user,
                                   const std::string& path,
                                   const std::string& content) {
    if (pathEscapesRoot(path)) return false;
    std::string p = normalizePath(path);
    if (isRootOrSharedRoot(p)) return false;

    auto parts = splitPath(p);
    if (parts.size() < 2) return false;
    if (parts[0] != "personal") return false;

    ensureUser(user);
    DirEntry* dir = &user_personal_[user];
    for (size_t i = 1; i < parts.size() - 1; ++i) {
        auto it = dir->subdirs.find(parts[i]);
        if (it == dir->subdirs.end()) return false;
        dir = &user_personal_[it->second];
    }

    std::string name = parts.back();
    if (dir->subdirs.count(name)) return false;

    FileEntry fe{content, user};
    dir->files[name] = fe;

    std::string share_key = getShareKey(user, p);
    auto it = shares_.find(share_key);
    if (it != shares_.end()) {
        for (const auto& target : it->second) {
            ensureUser(target);
            DirEntry& shared_root = user_shared_[target];
            auto subit = shared_root.subdirs.find(user);
            if (subit != shared_root.subdirs.end()) {
                user_shared_[subit->second].files[name] = fe;
            }
        }
    }
    return true;
}

bool VirtualFileSystem::writeFile(const std::string& user,
                                  const std::string& path,
                                  const std::string& content) {
    if (pathEscapesRoot(path)) return false;

    std::string p = normalizePath(path);
    if (isRootOrSharedRoot(p)) return false;

    auto parts = splitPath(p);
    if (parts.size() < 2) return false;
    if (parts[0] != "personal") return false;

    ensureUser(user);

    DirEntry* dir = &user_personal_[user];

    // EXACT same traversal as createFile
    for (size_t i = 1; i < parts.size() - 1; ++i) {
        auto it = dir->subdirs.find(parts[i]);
        if (it == dir->subdirs.end()) return false;
        dir = &user_personal_[it->second];
    }

    std::string name = parts.back();

    auto file_it = dir->files.find(name);
    if (file_it == dir->files.end()) return false;

    file_it->second.content = content;

    return true;
}

std::optional<std::string> VirtualFileSystem::readFile(
    const std::string& user,
    const std::string& path) const {

    if (pathEscapesRoot(path)) return std::nullopt;

    std::string p = normalizePath(path);
    auto parts = splitPath(p);

    if (parts.size() < 2) return std::nullopt;
    if (parts[0] != "personal" && parts[0] != "shared") return std::nullopt;

    // Determine parent directory
    std::string parentPath = p.substr(0, p.find_last_of('/'));
    if (parentPath.empty()) parentPath = "/";

    const DirEntry* dir = getDirAt(user, parentPath, parts[0] == "shared");
    if (!dir) return std::nullopt;

    auto it = dir->files.find(parts.back());
    if (it == dir->files.end()) return std::nullopt;

    return it->second.content;
}

std::string VirtualFileSystem::getShareKey(const std::string& owner,
                                           const std::string& path) const {
    return owner + "|" + path;
}

bool VirtualFileSystem::shareFile(const std::string& owner,
                                  const std::string& file_path,
                                  const std::string& target_user) {
    if (pathEscapesRoot(file_path)) return false;
    if (!userExists(target_user)) return false;

    std::string p = normalizePath(file_path);
    auto parts = splitPath(p);
    if (parts.size() < 2 || parts[0] != "personal") return false;

    const DirEntry* dir = getDirAt(owner, p, false);
    if (!dir) {
        std::string parent = "/";
        for (size_t i = 0; i < parts.size() - 1; ++i)
            parent += (i > 0 ? "/" : "") + parts[i];
        dir = getDirAt(owner, parent, false);
    }
    if (!dir) return false;

    auto it = dir->files.find(parts.back());
    if (it == dir->files.end()) return false;

    std::string share_key = getShareKey(owner, p);
    shares_[share_key].insert(target_user);

    ensureUser(target_user);
    DirEntry& shared_root = user_shared_[target_user];
    if (shared_root.subdirs.find(owner) == shared_root.subdirs.end()) {
        std::string sub_key = target_user + ":shared/" + owner;
        user_shared_[sub_key] = DirEntry{};
        shared_root.subdirs[owner] = sub_key;
    }
    DirEntry& target_dir = user_shared_[shared_root.subdirs[owner]];
    target_dir.files[parts.back()] = it->second;

    return true;
}

std::vector<std::string> VirtualFileSystem::getAllUsers() const {
    std::vector<std::string> users;

    for (const auto& [key, _] : user_personal_) {
        if (key.find(':') == std::string::npos)
            users.push_back(key);
    }

    return users;
}

bool VirtualFileSystem::userExists(const std::string& username) const {
    return user_personal_.find(username) != user_personal_.end() &&
           username.find(':') == std::string::npos;
}

static std::string unescape(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            r += s[++i];
        } else {
            r += s[i];
        }
    }
    return r;
}

static std::vector<std::string> splitLine(const std::string& line, char sep) {
    std::vector<std::string> parts;
    std::string cur;
    bool escape = false;
    for (char c : line) {
        if (escape) { cur += c; escape = false; continue; }
        if (c == '\\') { escape = true; continue; }
        if (c == sep) { parts.push_back(cur); cur.clear(); continue; }
        cur += c;
    }
    parts.push_back(cur);
    return parts;
}

std::string VirtualFileSystem::serialize() const {
    std::ostringstream os;
    auto esc = [](const std::string& s) {
        std::string r;
        for (char c : s) {
            if (c == '|' || c == '\n' || c == '\\') r += '\\', r += c;
            else r += c;
        }
        return r;
    };

    for (const auto& [token, user] : token_registry_) {
        os << "T:" << esc(token) << "|" << esc(user) << "\n";
    }

    std::set<std::string> top_users;
    for (const auto& [key, _] : user_personal_) {
        if (key.find(':') == std::string::npos)
            top_users.insert(key);
    }

    for (const auto& u : top_users) {
        os << "U:" << esc(u) << "\n";
    }

    auto getPrefix = [](const std::string& key, bool personal) {
    if (key.find(':') == std::string::npos) {
        return std::string(personal ? "personal/" : "shared/") + key;
    }

    size_t colon = key.find(':');
    std::string user = key.substr(0, colon);
    std::string rest = key.substr(colon + 1);

    std::vector<std::string> parts;
    std::stringstream ss(rest);
    std::string p;

    while (std::getline(ss, p, '/')) {
        if (!p.empty()) parts.push_back(p);
    }

    std::string base = personal ? "personal" : "shared";
    std::string result = base + "/" + user;

    for (size_t i = 1; i < parts.size(); ++i)
        result += "/" + parts[i];

    return result;
    };

    auto serDir = [&](const DirEntry& d, const std::string& prefix) {
        for (const auto& [name, sub] : d.subdirs) {
            os << "D:" << esc(prefix) << "|" << esc(name) << "|" << esc(sub) << "\n";
        }
        for (const auto& [name, fe] : d.files) {
            os << "F:" << esc(prefix) << "|" << esc(name) << "|" << esc(fe.owner) << "|"
               << esc(fe.content) << "\n";
        }
    };

    for (const auto& [key, dir] : user_personal_) {
        serDir(dir, getPrefix(key, true));
    }
    for (const auto& [key, dir] : user_shared_) {
        serDir(dir, getPrefix(key, false));
    }

    for (const auto& [key, users] : shares_) {
        for (const auto& u : users) {
            os << "S:" << esc(key) << "|" << esc(u) << "\n";
        }
    }

    return os.str();
}

bool VirtualFileSystem::deserialize(const std::string& data) {
    user_personal_.clear();
    user_shared_.clear();
    shares_.clear();
    token_registry_.clear();

    std::istringstream is(data);
    std::string line;
    while (std::getline(is, line)) {
        if (line.empty()) continue;
        if (line.compare(0, 2, "U:") == 0) {
            std::string user = unescape(line.substr(2));
            ensureUser(user);
        } else if (line.compare(0, 2, "T:") == 0) {
            auto parts = splitLine(line.substr(2), '|');
            if (parts.size() != 2) continue;
            token_registry_[unescape(parts[0])] = unescape(parts[1]);
        } else if (line.compare(0, 2, "D:") == 0) {
            auto parts = splitLine(line.substr(2), '|');
            if (parts.size() != 3) continue;
            std::string prefix = unescape(parts[0]);
            std::string name = unescape(parts[1]);
            std::string subkey = unescape(parts[2]);
            auto pd = splitPath(prefix);
            if (pd.size() < 2) continue;
            std::string dtype = pd[0];
            std::string user = pd[1];
            if (dtype == "personal") {
                ensureUser(user);
                DirEntry* dir = &user_personal_[user];
                for (size_t i = 2; i < pd.size(); ++i) {
                    auto it = dir->subdirs.find(pd[i]);
                    if (it == dir->subdirs.end()) {
                        std::string sk = user + ":personal";
                        for (size_t j = 1; j <= i; ++j) sk += "/" + pd[j];
                        user_personal_[sk] = DirEntry{};
                        dir->subdirs[pd[i]] = sk;
                    }
                    dir = &user_personal_[dir->subdirs[pd[i]]];
                }
                user_personal_[subkey] = DirEntry{};
                dir->subdirs[name] = subkey;
            } else if (dtype == "shared") {
                ensureUser(user);
                DirEntry* dir = &user_shared_[user];
                for (size_t i = 2; i < pd.size(); ++i) {
                    auto it = dir->subdirs.find(pd[i]);
                    if (it == dir->subdirs.end()) {
                        std::string sk = user + ":shared";
                        for (size_t j = 1; j <= i; ++j) sk += "/" + pd[j];
                        user_shared_[sk] = DirEntry{};
                        dir->subdirs[pd[i]] = sk;
                    }
                    dir = &user_shared_[dir->subdirs[pd[i]]];
                }
                user_shared_[subkey] = DirEntry{};
                dir->subdirs[name] = subkey;
            }
        } else if (line.compare(0, 2, "F:") == 0) {
            auto parts = splitLine(line.substr(2), '|');
            if (parts.size() != 4) continue;
            std::string prefix = unescape(parts[0]);
            std::string name = unescape(parts[1]);
            std::string owner = unescape(parts[2]);
            std::string content = unescape(parts[3]);
            auto pd = splitPath(prefix);
            if (pd.size() < 2) continue;
            std::string dtype = pd[0];
            std::string user = pd[1];
            if (dtype == "personal") {
                ensureUser(user);
                DirEntry* dir = &user_personal_[user];
                for (size_t i = 2; i < pd.size(); ++i) {
                    dir = &user_personal_[dir->subdirs.at(pd[i])];
                }
                dir->files[name] = FileEntry{content, owner};
            } else if (dtype == "shared") {
                ensureUser(user);
                DirEntry* dir = &user_shared_[user];
                for (size_t i = 2; i < pd.size(); ++i) {
                    dir = &user_shared_[dir->subdirs.at(pd[i])];
                }
                dir->files[name] = FileEntry{content, owner};
            }
        } else if (line.compare(0, 2, "S:") == 0) {
            auto parts = splitLine(line.substr(2), '|');
            if (parts.size() != 2) continue;
            shares_[unescape(parts[0])].insert(unescape(parts[1]));
        }
    }
    return true;
}

bool VirtualFileSystem::load(const std::string& base_path) {
    base_path_ = base_path;
    std::string meta_path = base_path + "/metadata";
    std::ifstream f(meta_path);
    if (!f) return false;

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();

    return deserialize(content);
}

void VirtualFileSystem::save() {
    std::string content = serialize();
    std::string meta_path = base_path_ + "/metadata";
    std::ofstream f(meta_path);
    if (f) f << content;
}

}
