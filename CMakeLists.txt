cmake_minimum_required(VERSION 3.11)

project(njp.cxx)

kls_add_executable_module(njp.count)
kls_module_source_directory(njp.count Module)
target_link_libraries(
        njp.count PUBLIC
        njpxx::common
        klsxx::essential klsxx::coroutine klsxx::thread klsxx::io klsxx::phttp klsxx::journal
)
