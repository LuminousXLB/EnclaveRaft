# EnclaveRaft
Implement Raft in an Enclave

## Prerequisites

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
