# EnclaveRaft
Implement Raft in an Enclave

## How to run

### Prerequisites

- SGX-SDK
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

### Build

```console
$ mkdir build
$ cd build
$ cmake ..
$ make
```

#### Server

```console
$ ./App_raft <server-id>
```

#### Client

```console
$ ./client <client-id> <request interval (ms)> <payload size (bytes)>
```

