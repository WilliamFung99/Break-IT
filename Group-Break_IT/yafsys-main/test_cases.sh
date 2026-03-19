#!/bin/bash
# YAFsys - Full Security & Functional Test Suite
# Author: Gayathri
# Tests all functional requirements from the BIBIFI Build-It spec

BINARY="./fileserver"
PASS=0
FAIL=0

run_test() {
    local name="$1"
    local input="$2"
    local expected="$3"
    local keyfile="$4"

    actual=$(echo "$input" | $BINARY "$keyfile" 2>/dev/null)
    if echo "$actual" | grep -qF "$expected"; then
        echo "PASS: $name"
        ((PASS++))
    else
        echo "FAIL: $name"
        echo "  Expected to contain: $expected"
        echo "  Got: $actual"
        ((FAIL++))
    fi
}

echo "=============================="
echo " YAFsys Test Suite"
echo "=============================="

# Setup - fresh run
rm -rf filesystem admin_keyfile alice_keyfile bob_keyfile charlie_keyfile 2>/dev/null

# TEST 1: First run creates filesystem and admin_keyfile
$BINARY admin_keyfile << 'EOF' > /dev/null
exit
EOF
if [ -f "admin_keyfile" ] && [ -d "filesystem" ]; then
    echo "PASS: First run creates filesystem and admin_keyfile"
    ((PASS++))
else
    echo "FAIL: First run did not create filesystem or admin_keyfile"
    ((FAIL++))
fi

# TEST 2: Successful admin login
run_test "Admin login success" "exit" "Logged in as admin" "admin_keyfile"

# TEST 3: Invalid keyfile
echo "baduser
badtoken" > fake_keyfile
actual=$($BINARY fake_keyfile 2>/dev/null)
if [ "$actual" = "Invalid keyfile" ]; then
    echo "PASS: Invalid keyfile rejected"
    ((PASS++))
else
    echo "FAIL: Invalid keyfile not rejected — got: $actual"
    ((FAIL++))
fi
rm fake_keyfile

# TEST 4: Second login persists (not Invalid keyfile)
run_test "Second login persists" "exit" "Logged in as admin" "admin_keyfile"

# TEST 5: adduser creates keyfile
$BINARY admin_keyfile << 'EOF' > /dev/null
adduser alice
adduser bob
exit
EOF
if [ -f "alice_keyfile" ] && [ -f "bob_keyfile" ]; then
    echo "PASS: adduser creates keyfiles"
    ((PASS++))
else
    echo "FAIL: adduser did not create keyfiles"
    ((FAIL++))
fi

# TEST 6: Duplicate user
run_test "Duplicate adduser" "adduser alice
exit" "User alice already exists" "admin_keyfile"

# TEST 7: Non-admin cannot adduser
run_test "Non-admin adduser blocked" "adduser charlie
exit" "Invalid Command" "alice_keyfile"

# TEST 8: User login
run_test "User login" "exit" "Logged in as alice" "alice_keyfile"

# TEST 9: pwd at root
run_test "pwd at root" "pwd
exit" "/" "alice_keyfile"

# TEST 10: ls at root shows personal and shared
run_test "ls at root shows personal" "ls
exit" "personal" "alice_keyfile"

run_test "ls at root shows shared" "ls
exit" "shared" "alice_keyfile"

# TEST 11: mkdir and cd
run_test "mkdir and cd" "cd personal
mkdir docs
cd docs
pwd
exit" "/personal/docs" "alice_keyfile"

# TEST 12: mkfile and cat
run_test "mkfile and cat" "cd personal
mkfile hello.txt world
cat hello.txt
exit" "world" "alice_keyfile"

# TEST 13: cat nonexistent file
run_test "cat nonexistent" "cd personal
cat ghost.txt
exit" "ghost.txt doesn't exist" "alice_keyfile"

# TEST 14: mkdir duplicate
run_test "mkdir duplicate" "cd personal
mkdir docs
exit" "Directory already exists" "alice_keyfile"

# TEST 15: mkfile overwrite
run_test "mkfile overwrite" "cd personal
mkfile hello.txt updated
cat hello.txt
exit" "updated" "alice_keyfile"

# TEST 16: Forbidden - mkdir in root
run_test "Forbidden mkdir in root" "mkdir newdir
exit" "Forbidden" "alice_keyfile"

# TEST 17: Forbidden - mkfile in root
run_test "Forbidden mkfile in root" "mkfile newfile.txt x
exit" "Forbidden" "alice_keyfile"

# TEST 18: Forbidden - mkdir in shared
run_test "Forbidden mkdir in shared" "cd shared
mkdir hackdir
exit" "Forbidden" "alice_keyfile"

# TEST 19: Forbidden - mkfile in shared
run_test "Forbidden mkfile in shared" "cd shared
mkfile hackfile.txt x
exit" "Forbidden" "alice_keyfile"

# TEST 20: share file then bob reads it
$BINARY alice_keyfile << 'EOF' > /dev/null
cd personal
mkfile secret.txt topsecret
share secret.txt bob
exit
EOF
run_test "Bob reads shared file" "cd shared
cd alice
cat secret.txt
exit" "topsecret" "bob_keyfile"

# TEST 21: share - file checked before user
run_test "Share checks file first" "cd personal
share fakefile.txt bob
exit" "File fakefile.txt doesn't exist" "alice_keyfile"

# TEST 22: share - user not found
run_test "Share checks user after file" "cd personal
share secret.txt nobody
exit" "User nobody doesn't exist" "alice_keyfile"

# TEST 23: shared dir is read-only
run_test "Shared dir read-only mkfile" "cd shared
cd alice
mkfile hack.txt x
exit" "Forbidden" "bob_keyfile"

# TEST 24: mkfile update propagates to shared
$BINARY alice_keyfile << 'EOF' > /dev/null
cd personal
mkfile secret.txt UPDATED
exit
EOF
run_test "Share update propagates" "cd shared
cd alice
cat secret.txt
exit" "UPDATED" "bob_keyfile"

# TEST 25: path traversal blocked
run_test "Path traversal blocked" "cd ../../../../../../../../etc
pwd
exit" "/" "alice_keyfile"

# TEST 26: cd to nonexistent stays in place
run_test "cd nonexistent stays put" "cd personal
cd nonexistent
pwd
exit" "/personal" "alice_keyfile"

# TEST 27: multi-level cd
run_test "Multi-level cd" "cd personal/docs
pwd
exit" "/personal/docs" "alice_keyfile"

# TEST 28: cd .. works
run_test "cd .. works" "cd personal
cd docs
cd ..
pwd
exit" "/personal" "alice_keyfile"

# TEST 29: ls shows . and ..
run_test "ls shows dot entries" "ls
exit" "d -> ." "alice_keyfile"

# TEST 30: invalid command
run_test "Invalid command" "fakecommand
exit" "Invalid Command" "alice_keyfile"

# TEST 31: admin sees all users
run_test "Admin ls shows all users" "ls
exit" "alice" "admin_keyfile"

# TEST 32: admin can cd into user and read files
run_test "Admin reads user files" "cd alice
cd personal
cat hello.txt
exit" "updated" "admin_keyfile"

# TEST 33: write command updates existing file
run_test "write command" "cd personal
write hello.txt newcontent
cat hello.txt
exit" "newcontent" "alice_keyfile"

# TEST 34: write on nonexistent file fails
run_test "write nonexistent fails" "cd personal
write ghostfile.txt x
exit" "File doesn't exist" "alice_keyfile"

# TEST 35: deep nested dir persists across sessions
$BINARY alice_keyfile << 'EOF' > /dev/null
cd personal/docs
mkdir subdir
cd subdir
mkfile deep.txt deepvalue
exit
EOF
run_test "Deep nested dir persists" "cd personal/docs/subdir
cat deep.txt
exit" "deepvalue" "alice_keyfile"

echo ""
echo "=============================="
echo " Results: $PASS passed, $FAIL failed"
echo "=============================="
