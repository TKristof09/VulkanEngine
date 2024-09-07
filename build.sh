cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -S . -B .cmake/
# workaround for clangd, create makefiles in two diff folders and only use make on one of them because
# clangd doesnt work for some reason after calling make, so this way clangd will still have the files untouched
# the problem might be with the precompiled header, but im too lazy to investigate right now
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -S . -B .cmake2/
cd .cmake2
make -j
cd ..
# compdb -p .cmake/ list > compile_commands.json
