# YAFsys (Yet-Another-FileSystem)

C++17 virtual filesystem. Token-based keyfile auth. No third-party deps.

## Build (Ubuntu)

```bash
sudo apt-get install -y build-essential cmake
mkdir -p build && cd build
cmake ..
make
```

## Run

```bash
./fileserver keyfile_name
```

First run creates `filesystem/` and `admin_keyfile` in cwd. Keyfiles (sensitive) must be in project root.
Invalid keyfile: prints "Invalid keyfile" and exits. Valid: "Logged in as {username}".

## Commands

| Command | Description |
|---------|-------------|
| cd \<dir\> | Change dir. Supports . , .. , multi-level |
| pwd | Current directory |
| ls | List (d = dir, f = file) |
| cat \<file\> | Show contents |
| share \<file\> \<user\> | Share with user, read-only in their /shared |
| mkdir \<dir\> | Create directory |
| mkfile \<file\> \<contents\> | Create or overwrite file |
| mkfile \<file\> | Create without content |
| write \<file\> \<contents\>| Write contents to a file |
| exit | Quit |
| adduser \<user\> | Admin only. Create user keyfile |

## Limits

- No mkdir/mkfile in / or /shared
- /shared is read-only
- Root has /personal and /shared only
