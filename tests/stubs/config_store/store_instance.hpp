#pragma once

#include <option/has_e2ee_support.h>
#if HAS_E2EE_SUPPORT()
    #include <e2ee/identity_check_levels.hpp>
#endif
#include <common/hw_check.hpp>
#include <option/xl_enclosure_support.h>
#include <option/has_chamber_filtration_api.h>
#include <cstdint>
#include <array>

// Mocked config store with the one variable we need. Not persistent.

struct ConfigStore {
    struct HostName {
        void set(const char *n) {
        }
        const char *get() const {
            return "nice_hostname";
        }
        const char *get_c_str() const {
            return "nice_hostname";
        }
    };

    struct ProxyHost {
        std::array<char, 10> get() const {
            return {};
        }
    };

    struct BoolTrue {
        bool get() const {
            return true;
        }
    };

    struct IntZero {
        int get() const {
            return 0;
        }
    };

    HostName hostname;
    BoolTrue verify_gcode;

    ProxyHost connect_proxy_host;
    IntZero connect_proxy_port;
#if HAS_E2EE_SUPPORT()
    struct Value {
        void set(e2ee::IdentityCheckLevel n) {
        }
        e2ee::IdentityCheckLevel get() const {
            return e2ee::IdentityCheckLevel::AnyIdentity;
        }
    };

    Value identity_check;
#endif
    struct HwCheck {
        HWCheckSeverity get() const {
            return HWCheckSeverity::Abort;
        }
    };
    HwCheck hw_check_input_shaper;
    HwCheck hw_check_firmware;
    HwCheck hw_check_gcode_level;
    HwCheck hw_check_nozzle;
    HwCheck hw_check_model;

    float get_nozzle_diameter(uint8_t) const {
        return 0.4f;
    }

    struct ManyFalse {
        std::array<bool, 5> get() const {
            return { false, false, false, false, false };
        }
    };

    ManyFalse nozzle_is_hardened;
    ManyFalse nozzle_is_high_flow;

#if XL_ENCLOSURE_SUPPORT() && HAS_CHAMBER_FILTRATION_API()
    // Written by SET_VALUE handling in the Connect planner; nothing here reads them back.
    struct BoolSink {
        void set(bool) {
        }
        bool get() const {
            return false;
        }
    };

    struct UintSink {
        void set(uint32_t) {
        }
        uint32_t get() const {
            return 0;
        }
    };

    BoolSink chamber_print_filtration_enable;
    BoolSink chamber_post_print_filtration_enable;
    UintSink chamber_post_print_filtration_duration_min;
#endif
};

inline ConfigStore &config_store() {
    static ConfigStore store;
    return store;
}
