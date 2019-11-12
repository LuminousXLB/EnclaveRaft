cmake_minimum_required(VERSION 3.10)
project(EnclaveRaft)

set(CMAKE_CXX_STANDARD 14)

# Intel SGX
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(SGX REQUIRED)
include_directories(${SGX_INCLUDE_DIR})

##### third_pary ######################################################################################################

add_subdirectory(third_party/spdlog-1.4.2)
add_subdirectory(third_party/cppcodec-0.2)
include_directories(third_party/cppcodec-0.2)
add_subdirectory(third_party/json11)
include_directories(third_party/json11)

#######################################################################################################################

add_subdirectory(raft_app/sgx_utils)
include_directories(raft_app/sgx_utils)

include_directories(common)
set(T_COMMON_SRCS common/aes_ctr_crypto.cxx)
set(U_COMMON_SRCS "")
set(C_COMMON_SRCS common/aes_ctr_crypto.cxx)

#######################################################################################################################
file(GLOB T_CORNERSTONE_SRCS raft_enclave/raft/src/*.cxx)
file(GLOB T_RAFT_LOGSTORE_SRCS raft_enclave/app_impl/*.cxx)
file(GLOB T_SGX_PORT_SRCS raft_enclave/port/*.cxx)
file(GLOB T_SGX_SYSTEM_SRCS raft_enclave/system/*.cxx)

set(EDL_SEARCH_PATHS raft_enclave)

# build enclave library
add_enclave_library(Enclave_raft
        SRCS raft_enclave/raft_enclave.cpp ${T_COMMON_SRCS} ${T_CORNERSTONE_SRCS} ${T_SGX_PORT_SRCS} ${T_RAFT_LOGSTORE_SRCS} ${T_SGX_SYSTEM_SRCS}
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
file(GLOB U_SGX_SYSTEM_SRCS raft_app/system/*.cxx)

# build untrusted executable to run with enclave
add_untrusted_executable(App_raft
        SRCS raft_app/raft_app.cpp ${U_COMMON_SRCS} ${U_SGX_PORT_SRCS} ${U_SGX_SYSTEM_SRCS}
        EDL raft_enclave/raft_enclave.edl
        EDL_SEARCH_PATHS raft_enclave)

target_link_libraries(App_raft sgx_utils spdlog::spdlog json11 OpenSSL::SSL)
target_include_directories(App_raft PRIVATE third_party/spdlog-1.4.2/include)
target_include_directories(App_raft PRIVATE third_party/asio-1.12.2/include)

add_dependencies(App_raft Enclave_raft-sign)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/logs")

#######################################################################################################################
find_package(OpenSSL)

file(GLOB TEST_CLIENT_SRCS client/*.cxx)

add_executable(client ${TEST_CLIENT_SRCS} ${C_COMMON_SRCS} raft_enclave/raft/src/buffer.cxx)
target_link_libraries(client PRIVATE OpenSSL::Crypto)
target_link_libraries(client PRIVATE spdlog::spdlog)
target_include_directories(client PRIVATE third_party/spdlog-1.4.2/include)
target_include_directories(client PRIVATE third_party/asio-1.12.2/include)
