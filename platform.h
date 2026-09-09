//
// Copyright (c) 2026 Nightwind
//

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <direct.h>

    #define PATH_MAX _MAX_PATH

    #define usleep(x) Sleep((x) < 1000 ? 1 : (x) / 1000)
    #define mkdir(path, mode) _mkdir(path)

    #ifdef _MSC_VER
        #define __builtin_bswap16(x) _byteswap_ushort(x)
        #define __builtin_bswap32(x) _byteswap_ulong(x)
    #endif // _MSC_VER

    typedef SSIZE_T ssize_t;

    static inline void GetHomeDirectory(char *out, size_t size_out) {
        char *home = NULL;
        size_t length = 0;

        if (_dupenv_s(&home, &length, "USERPROFILE") == 0 && home) {
            snprintf(out, size_out, "%s", home);
            free(home);
        } else {
            snprintf(out, size_out, "C:\\Users");
        }
    }
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <unistd.h>

    static inline void GetHomeDirectory(char *out, size_t size_out) {
        const char *home = getenv("HOME");
        snprintf(out, size_out, "%s", home ? home : "/tmp");
    }
#endif

#ifdef _WIN32
    static inline void GetExecutableDirectory(char *out, size_t size_out) {
        char path[PATH_MAX];
        GetModuleFileNameA(NULL, path, PATH_MAX);
        
        char *last_character = strrchr(path, '\\');
        if (last_character) {
            *last_character = '\0';
        }

        snprintf(out, size_out, "%s", path);
    }
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>

    static inline void GetExecutableDirectory(char *out, size_t size_out) {
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            char *last_character = strrchr(path, '/');
            if (last_character) {
                *last_character = '\0';
            }
            
            snprintf(out, size_out, "%s", path);
        }
    }
#else
    static inline void GetExecutableDirectory(char *out, size_t size_out) {
        char path[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
        if (count != -1) {
            path[count] = '\0';

            char *last_character = strrchr(path, '/');
            if (last_character) {
                *last_character = '\0';
            }
            
            snprintf(out, size_out, "%s", path);
        }
    }
#endif

#endif // PLATFORM_H
