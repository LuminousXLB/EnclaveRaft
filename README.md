# EnclaveRaft
Implement Raft in an Enclave

## Prerequisites

- EPID

Head to  https://api.portal.trustedservices.intel.com/EPID-attestation to apply for an SGX Attestation Service subscription.

Create `EnclaveRaft\common\secret.h`

```cpp
#pragma once

#define intel_api_primary "{{ Primary key }}"
#define intel_api_secondary "{{ Secondary key }}"
#define intel_api_spid "{{ SPID }}"

```


- SGX-SDK 2.9.1
<https://github.com/intel/linux-sgx/>
- SGX-SSL
<https://github.com/intel/intel-sgx-ssl>

If the sgx-sdk is not installed in  `/opt/intel/sgxsdk`, specify the path using 
```cmake
set(SGX_DIR <sgxsdk-path>)
```

If the sgx-sdk is not installed in  `/opt/intel/sgxssl`, specify the path using 
```cmake
set(SGXSSL_SDK <sgxssl-path>)
```

## Build & Run

```console
$ mkdir build
$ cd build
$ cmake ..
$ make
```

#### Run the Server

```console
$ ./App_raft <server-id>
```

#### Run the Client

```console
$ ./client <client-id> <request interval (ms)> <payload size (bytes)>
```

Note: The default client routine in `client/client.cxx` assumes that there have been 5 servers with ID 1-5 running.
