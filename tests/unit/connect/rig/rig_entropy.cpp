// A working entropy source for the rig, replacing the stub in
// tests/unit/lib/WUI/nhttp/missing_functions.c that returns 42 and fills nothing.
//
// The firmware's mbedtls config sets MBEDTLS_ENTROPY_HARDWARE_ALT and MBEDTLS_NO_PLATFORM_ENTROPY,
// so mbedtls will not use its own POSIX source and calls mbedtls_hardware_poll instead - on the
// printer that is the STM32's RNG (src/common/hardware_rng.cpp). The stub satisfies the linker for
// tests that never seed a DRBG, but for the rig it makes mbedtls_ctr_drbg_seed fail, and tls.cpp
// then returns before opening a socket. The symptom is a TLS run that connects to nothing at all,
// with no error anywhere, because the rig's logging is mocked out.
//
// /dev/urandom rather than arc4random_buf, so this is the same on the Linux container rig.

#include <cstddef>
#include <cstdio>

extern "C" int mbedtls_hardware_poll(void *, unsigned char *output, size_t len, size_t *olen) {
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom == nullptr) {
        return -1;
    }

    const size_t read = fread(output, 1, len, urandom);
    fclose(urandom);

    if (read != len) {
        return -1;
    }

    *olen = read;
    return 0;
}
