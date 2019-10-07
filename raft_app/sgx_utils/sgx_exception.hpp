//
// Created by ncl on 17/9/19.
//

#ifndef SGX_ENCLAVE_TO_ENCLAVE_RA_SGX_ERROR_H
#define SGX_ENCLAVE_TO_ENCLAVE_RA_SGX_ERROR_H

#include <sgx_urts.h>
#include <string>
#include <exception>

using namespace std;

class sgx_error : public exception {
    string info;
public:
    sgx_error(const string &where, sgx_status_t status) {
        char buffer[4096];
        sprintf(buffer, "%s: %04x %s", where.c_str(), status, error_str(status).c_str());
        info = buffer;
    }

    const char *what() const noexcept override {
        return info.c_str();
    }

private:
    string error_str(sgx_status_t status) {
        switch (status) {
            case SGX_ERROR_UNEXPECTED:
                return "Unexpected error";
            case SGX_ERROR_INVALID_PARAMETER:
                return "The parameter is incorrect";
            case SGX_ERROR_OUT_OF_MEMORY:
                return "Not enough memory is available to complete this operation";
            case SGX_ERROR_ENCLAVE_LOST:
                return "Enclave lost after power transition or used in child process created by linux:fork()";
            case SGX_ERROR_INVALID_STATE:
                return "SGX API is invoked in incorrect order or state";
            case SGX_ERROR_FEATURE_NOT_SUPPORTED:
                return "Feature is not supported on this platform";


            case SGX_ERROR_INVALID_FUNCTION:
                return "The ecall/ocall index is invalid";
            case SGX_ERROR_OUT_OF_TCS:
                return "The enclave is out of TCS";
            case SGX_ERROR_ENCLAVE_CRASHED:
                return "The enclave is crashed";
            case SGX_ERROR_ECALL_NOT_ALLOWED:
                return "The ECALL is not allowed at this time, e.g. ecall is blocked by the dynamic entry table, or nested ecall is not allowed during initialization";
            case SGX_ERROR_OCALL_NOT_ALLOWED:
                return "The OCALL is not allowed at this time, e.g. ocall is not allowed during exception handling";
            case SGX_ERROR_STACK_OVERRUN:
                return "The enclave is running out of stack";

            case SGX_ERROR_UNDEFINED_SYMBOL:
                return "The enclave image has undefined symbol.";
            case SGX_ERROR_INVALID_ENCLAVE:
                return "The enclave image is not correct.";
            case SGX_ERROR_INVALID_ENCLAVE_ID:
                return "The enclave id is invalid";
            case SGX_ERROR_INVALID_SIGNATURE:
                return "The signature is invalid";
            case SGX_ERROR_NDEBUG_ENCLAVE:
                return "The enclave is signed as product enclave, and can not be created as debuggable enclave.";
            case SGX_ERROR_OUT_OF_EPC:
                return "Not enough EPC is available to load the enclave";
            case SGX_ERROR_NO_DEVICE:
                return "Can't open SGX device";
            case SGX_ERROR_MEMORY_MAP_CONFLICT:
                return "Page mapping failed in driver";
            case SGX_ERROR_INVALID_METADATA:
                return "The metadata is incorrect.";
            case SGX_ERROR_DEVICE_BUSY:
                return "Device is busy, mostly EINIT failed.";
            case SGX_ERROR_INVALID_VERSION:
                return "Metadata version is inconsistent between uRTS and sgx_sign or uRTS is incompatible with current platform.";
            case SGX_ERROR_MODE_INCOMPATIBLE:
                return "The target enclave 32/64 bit mode or sim/hw mode is incompatible with the mode of current uRTS.";
            case SGX_ERROR_ENCLAVE_FILE_ACCESS:
                return "Can't open enclave file.";
            case SGX_ERROR_INVALID_MISC:
                return "The MiscSelct/MiscMask settings are not correct";
            case SGX_ERROR_INVALID_LAUNCH_TOKEN:
                return "The launch token is not correct";

            case SGX_ERROR_MAC_MISMATCH:
                return "Indicates verification error for reports, sealed datas, etc";
            case SGX_ERROR_INVALID_ATTRIBUTE:
                return "The enclave is not authorized";
            case SGX_ERROR_INVALID_CPUSVN:
                return "The cpu svn is beyond platform's cpu svn value";
            case SGX_ERROR_INVALID_ISVSVN:
                return "The isv svn is greater than the enclave's isv svn";
            case SGX_ERROR_INVALID_KEYNAME:
                return "The key name is an unsupported value";

            case SGX_ERROR_SERVICE_UNAVAILABLE:
                return "Indicates aesm didn't respond or the requested service is not supported";
            case SGX_ERROR_SERVICE_TIMEOUT:
                return "The request to aesm timed out";
            case SGX_ERROR_AE_INVALID_EPIDBLOB:
                return "Indicates epid blob verification error";
            case SGX_ERROR_SERVICE_INVALID_PRIVILEGE:
                return "Enclave has no privilege to get launch token";
            case SGX_ERROR_EPID_MEMBER_REVOKED:
                return "The EPID group membership is revoked.";
            case SGX_ERROR_UPDATE_NEEDED:
                return "SGX needs to be updated";
            case SGX_ERROR_NETWORK_FAILURE:
                return "Network connecting or proxy setting issue is encountered";
            case SGX_ERROR_AE_SESSION_INVALID:
                return "Session is invalid or ended by server";
            case SGX_ERROR_BUSY:
                return "The requested service is temporarily not availabe";
            case SGX_ERROR_MC_NOT_FOUND:
                return "The Monotonic Counter doesn't exist or has been invalided";
            case SGX_ERROR_MC_NO_ACCESS_RIGHT:
                return "Caller doesn't have the access right to specified VMC";
            case SGX_ERROR_MC_USED_UP:
                return "Monotonic counters are used out";
            case SGX_ERROR_MC_OVER_QUOTA:
                return "Monotonic counters exceeds quota limitation";
            case SGX_ERROR_KDF_MISMATCH:
                return "Key derivation function doesn't match during key exchange";
            case SGX_ERROR_UNRECOGNIZED_PLATFORM:
                return "EPID Provisioning failed due to platform not recognized by backend serve";
            case SGX_ERROR_UNSUPPORTED_CONFIG:
                return "The config for trigging EPID Provisiong or PSE Provisiong&LTP is invali";

            case SGX_ERROR_NO_PRIVILEGE:
                return "Not enough privilege to perform the operation";

            case SGX_ERROR_PCL_ENCRYPTED:
                return "trying to encrypt an already encrypted enclave";
            case SGX_ERROR_PCL_NOT_ENCRYPTED:
                return "trying to load a plain enclave using sgx_create_encrypted_enclave";
            case SGX_ERROR_PCL_MAC_MISMATCH:
                return "section mac result does not match build time mac";
            case SGX_ERROR_PCL_SHA_MISMATCH:
                return "Unsealed key MAC does not match MAC of key hardcoded in enclave binary";
            case SGX_ERROR_PCL_GUID_MISMATCH:
                return "GUID in sealed blob does not match GUID hardcoded in enclave binary";

            case SGX_ERROR_FILE_BAD_STATUS:
                return "The file is in bad status, run sgx_clearerr to try and fix it";
            case SGX_ERROR_FILE_NO_KEY_ID:
                return "The Key ID field is all zeros, can't re-generate the encryption key";
            case SGX_ERROR_FILE_NAME_MISMATCH:
                return "The current file name is different then the original file name (not allowed, substitution attack)";
            case SGX_ERROR_FILE_NOT_SGX_FILE:
                return "The file is not an SGX file";
            case SGX_ERROR_FILE_CANT_OPEN_RECOVERY_FILE:
                return "A recovery file can't be opened, so flush operation can't continue (only used when no EXXX is returned) ";
            case SGX_ERROR_FILE_CANT_WRITE_RECOVERY_FILE:
                return "A recovery file can't be written, so flush operation can't continue (only used when no EXXX is returned) ";
            case SGX_ERROR_FILE_RECOVERY_NEEDED:
                return "When openeing the file, recovery is needed, but the recovery process failed";
            case SGX_ERROR_FILE_FLUSH_FAILED:
                return "fflush operation (to disk) failed (only used when no EXXX is returned)";
            case SGX_ERROR_FILE_CLOSE_FAILED:
                return "fclose operation (to disk) failed (only used when no EXXX is returned)";


            case SGX_ERROR_UNSUPPORTED_ATT_KEY_ID:
                return "platform quoting infrastructure does not support the key";
            case SGX_ERROR_ATT_KEY_CERTIFICATION_FAILURE:
                return "Failed to generate and certify the attestation key";
            case SGX_ERROR_ATT_KEY_UNINITIALIZED:
                return "The platform quoting infrastructure does not have the attestation key available to generate quote";
            case SGX_ERROR_INVALID_ATT_KEY_CERT_DATA:
                return "TThe data returned by the platform library's sgx_get_quote_config() is invalid";
            case SGX_ERROR_PLATFORM_CERT_UNAVAILABLE:
                return "The PCK Cert for the platform is not available";

            case SGX_INTERNAL_ERROR_ENCLAVE_CREATE_INTERRUPTED:
                return "The ioctl for enclave_create unexpectedly failed with EINTR. ";
            default:
                return "Unexpected SGX status";
        }
    }
};

#endif //SGX_ENCLAVE_TO_ENCLAVE_RA_SGX_ERROR_H
