find_package(LLVM 14 REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

llvm_map_components_to_libnames(llvm_libs core support irreader BitWriter mcjit TransformUtils analysis)

add_executable(myclang myclang.cpp instrument.cpp)

target_link_libraries(myclang ${llvm_libs})