// TLS refused, for the connect rig only - replacing src/connect/tls/tls.cpp.
//
// The rig talks plain HTTP to a Connect server on the LAN (RigPrinter sets config.tls = false), so
// the TLS branch of CachedFactory is never taken at runtime - but its symbols still have to resolve
// at link time.
//
// The real tls.cpp cannot be built off-target regardless: it depends on a certificate array that
// only exists in firmware builds (a static_assert on its size fails on host, and `certificates` is
// undeclared), on top of relying on transitive <array>/<cassert> includes that differ here.
//
// Should the rig ever need TLS, the honest route is generating that certificate data for the host
// build rather than extending this file.

#include <connect/tls/tls.hpp>

// The tls class holds an mbedtls_net_context by value, so its constructor must resolve even though
// nothing ever calls it here. The real net_sockets.cpp cannot be built on host at all: it includes
// lwIP's sockets.h, whose struct sockaddr/msghdr/iovec collide with the host's own <sys/socket.h>.
mbedtls_net_context::mbedtls_net_context(uint8_t timeout_s)
    : plain_conn(timeout_s)
    , timeout_happened(false) {}

namespace connect_client {

tls::tls(uint8_t timeout_s, bool custom_cert_)
    : http::Connection(timeout_s)
    , net_context(timeout_s)
    , custom_cert(custom_cert_) {}

tls::~tls() = default;

std::optional<http::Error> tls::connection(const char *, uint16_t, const char *, uint16_t) {
    // Connect refused rather than aborted: a config asking for TLS produces an ordinary connection
    // error the client already knows how to report, instead of killing the process.
    return http::Error::Connect;
}

std::variant<size_t, http::Error> tls::tx(const uint8_t *, size_t) {
    return http::Error::Connect;
}

std::variant<size_t, http::Error> tls::rx(uint8_t *, size_t, bool) {
    return http::Error::Connect;
}

bool tls::poll_readable(uint32_t) {
    return false;
}

} // namespace connect_client
