find ./cmake-build-debug -maxdepth 1 -type s -regextype posix-extended -regex '\./[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}' -delete
find ./cmake-build-release -maxdepth 1 -type s -regextype posix-extended -regex '\./[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}' -delete
find ./cmake-build-relwithdebinfo -maxdepth 1 -type s -regextype posix-extended -regex '\./[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}' -delete
