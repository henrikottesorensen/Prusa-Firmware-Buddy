#pragma once
#if defined(__unix__) || defined(__APPLE__) || defined(__WIN32__)
    #include <stdio.h>
    #define log_debug(x, ...)    printf(__VA_ARGS__)
    #define log_error(x, ...)    printf(__VA_ARGS__)
    // connection_cache.cpp includes only this header and uses log_info, so the host branch was
    // incomplete - it could not be compiled off-target at all.
    #define log_info(x, ...)     printf(__VA_ARGS__)
    #define log_warning(x, ...)  printf(__VA_ARGS__)
    #define log_critical(x, ...) printf(__VA_ARGS__)
    #define LOG_COMPONENT_DEF(...)
    #define LOG_COMPONENT_REF(...)
#else
    #include <logging/log.hpp>
#endif
