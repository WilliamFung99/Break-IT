#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace fs {

struct FileEntry {
    std::string content;
    std::string owner;
};

struct DirEntry {
    std::map<std::string, std::string> subdirs;
    std::map<std::string, FileEntry> files;
};

using ShareMap = std::map<std::string, std::set<std::string>>;

class VirtualFileSystem {
public:
    VirtualFileSystem() = default;

    bool initialize(const std::string& base_path);
    bool load(const std::string& base_path);
    void save();

    std::map<std::string, std::string>& getTokenRegistry() { return token_registry_; }
    const std::map<std::string, std::string>& getTokenRegistry() const { return token_registry_; }
    void addTokenForUser(const std::string& token, const std::string& username);

    bool userExists(const std::string& username) const;
    void ensureUser(const std::string& username);

    std::string normalizePath(const std::string& path) const;
    std::string resolvePath(const std::string& current, const std::string& target) const;
    bool isValidPath(const std::string& path) const;
    bool isSharedPath(const std::string& path) const;
    bool isRootOrSharedRoot(const std::string& path) const;
    bool pathEscapesRoot(const std::string& path) const;

    bool directoryExists(const std::string& user, const std::string& path) const;
    bool fileExists(const std::string& user, const std::string& path) const;
    std::vector<std::pair<std::string, bool>> listDirectory(const std::string& user,
                                                            const std::string& path) const;
    bool createDirectory(const std::string& user, const std::string& path);
    bool createFile(const std::string& user, const std::string& path,
                    const std::string& content);
// writeFile: overwrites file content for authorized users
// readFile: returns file content if user has access (owner or shared)
    bool writeFile(const std::string& user,
               const std::string& path,
               const std::string& content);
    std::optional<std::string> readFile(const std::string& user,
                                        const std::string& path) const;
    bool shareFile(const std::string& owner, const std::string& file_path,
                   const std::string& target_user);

    std::string getBasePath() const { return base_path_; }
    std::vector<std::string> getAllUsers() const;

private:
    std::string base_path_;
    std::map<std::string, DirEntry> user_personal_;
    std::map<std::string, DirEntry> user_shared_;
    ShareMap shares_;
    std::map<std::string, std::string> token_registry_;

    DirEntry& getOrCreatePersonal(const std::string& user);
    const DirEntry* getDirAt(const std::string& user, const std::string& path,
                             bool prefer_shared) const;
    DirEntry* getMutableDirAt(const std::string& user, const std::string& path,
                              bool shared_only);
    std::string getShareKey(const std::string& owner, const std::string& path) const;

    std::string serialize() const;
    bool deserialize(const std::string& data);
};

}
#endif
