cmake_minimum_required(VERSION 3.10)
project(EnclaveRaft)

set(CMAKE_CXX_STANDARD 11)

# Intel SGX
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(SGX REQUIRED)
include_directories(${SGX_INCLUDE_DIR})

##### spdlog ##########################################################################################################

add_subdirectory(third_party/spdlog-1.4.2)
include_directories(third_party/spdlog-1.4.2/include)

#######################################################################################################################

add_subdirectory(raft_app/sgx_utils)
include_directories(raft_app/sgx_utils)

include_directories(common)

#######################################################################################################################
file(GLOB CORNERSTONE_SRCS raft_enclave/raft/src/*.cxx)
file(GLOB T_SGX_PORT_SRCS raft_enclave/port/*.cxx)

set(EDL_SEARCH_PATHS raft_enclave)

# build enclave library
add_enclave_library(Enclave_raft
        SRCS raft_enclave/raft_enclave.cpp ${CORNERSTONE_SRCS} ${T_SGX_PORT_SRCS}
        EDL raft_enclave/raft_enclave.edl
        EDL_SEARCH_PATHS raft_enclave)

target_include_directories(Enclave_raft PRIVATE ${SGXSSL_INCLUDE_DIR})
#include_directories(raft_enclave)

# sign the enclave, according to configurations one-step or two-step signing will be performed.
# default one-step signing output enclave name is target.signed.so, change it with OUTPUT option.
enclave_sign(Enclave_raft
        KEY raft_enclave/raft_enclave_private.pem
        CONFIG raft_enclave/raft_enclave.config.xml)

#######################################################################################################################

file(GLOB U_SGX_PORT_SRCS raft_app/port/*.cxx)

# build untrusted executable to run with enclave
add_untrusted_executable(App_raft
        SRCS raft_app/raft_app.cpp ${U_SGX_PORT_SRCS}
        EDL raft_enclave/raft_enclave.edl
        EDL_SEARCH_PATHS raft_enclave)

target_link_libraries(App_raft sgx_utils spdlog::spdlog)

add_dependencies(App_raft Enclave_raft-sign)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/logs")
