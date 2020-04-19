export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build/
cd build
make -j8
cd ..
compdb -p build/ list > compile_commands.json
