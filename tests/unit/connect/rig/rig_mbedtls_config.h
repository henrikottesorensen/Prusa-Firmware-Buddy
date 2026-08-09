#pragma once

// The firmware's own mbedtls config, with the one value that cannot survive a 64-bit host adjusted.
//
// Using the firmware config rather than mbedtls' stock defaults is the point: it is what gives the
// rig the printer's real TLS parameters - MBEDTLS_SSL_IN_CONTENT_LEN 1024 / OUT 512, ECDSA-only key
// exchange, TLS 1.2 only. With stock defaults the rig would speak 16384-byte records and anything
// measured here about record sizes would describe a different TLS stack.
//
// The exception is the entropy source table. cipher_config_ece.h picks 19 sources with the comment
// "we want the structure to fit into 512B", and tls.cpp static_asserts that budget. Both numbers
// assume 32-bit pointers: mbedtls_entropy_source_state is 20 bytes there and 40 bytes here, so the
// same 19 slots produce ~880 bytes on a host and the assert fails. 120 bytes of fixed overhead plus
// 40 per slot fits the same 512B budget at 9 slots, so that is what the rig uses - satisfying the
// firmware's own assert unmodified rather than relaxing it.
//
// Nothing else is changed. The entropy table only governs how many sources may be registered, which
// has no bearing on records, ciphersuites, certificate validation or the handshake.

#include "../../../../include/mbedtls/cipher_config_ece.h"

#undef MBEDTLS_ENTROPY_MAX_SOURCES
#define MBEDTLS_ENTROPY_MAX_SOURCES 9
