cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build/
cd build
make -j8
mv ./compile_commands.json ../compile_commands.json
cd ..
