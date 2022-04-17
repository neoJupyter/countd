cmake_minimum_required(VERSION 3.11)

include(klsxx/Build/Config.cmake)
project(Count)
set(KLS_DISABLE_TEST TRUE)

kls_configure()
add_subdirectory(klsxx)

add_executable(countd)
kls_target(countd)
kls_module_source_directory(countd Source)
kls_vcpkg_package(nlohmann-json)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(countd PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
target_link_libraries(countd PUBLIC klsxx::essential klsxx::coroutine klsxx::thread klsxx::io klsxx::phttp klsxx::journal)
