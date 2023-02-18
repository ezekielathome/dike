#rm -rf build
cmake -B build -D CMAKE_TOOLCHAIN_FILE=cmake/ubuntu-latest_x86.cmake -D CMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build