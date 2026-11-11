#pragma once
// Relocatable TUTORIAL_DATA_PATH / TUTORIAL_OUTPUT_PATH.
//
// CMakeLists.txt defines those macros as RELATIVE paths ("../data", "../output").
// To make them resolve relative to the executable (not the current working
// directory), this header is force-included into every tutorial target (see
// target_compile_options in CMakeLists.txt) and chdir()s to the directory of
// the running binary before main() starts.
//
//   Linux:   readlink("/proc/self/exe")
//   Windows: GetModuleFileNameA()
//
// Works for both layouts:
//   <root>/bin/*_bin          (Linux)   -> ../data = <root>/data
//   <root>/bin-windows/*.exe (Windows) -> ../data = <root>/data
// If you copy the binaries somewhere else, keep a "data" (and "output") folder
// as a sibling of the binary folder. If the macros are configured as ABSOLUTE
// paths, the chdir() is simply harmless.

#include <cstdlib>
#include <string>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define TUTORIAL_CH_DIR _chdir
#else
  #include <unistd.h>
  #define TUTORIAL_CH_DIR chdir
#endif

namespace tutorial_paths_detail {

inline std::string exeDir() {
    char buf[4096];
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (n == 0) return ".";
    std::string p(buf, n);
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = '\0';
    std::string p(buf);
#endif
    const auto pos = p.find_last_of("/\\");
    return pos == std::string::npos ? std::string(".") : p.substr(0, pos);
}

struct ChdirToExeDir {
    ChdirToExeDir() {
        const std::string d = exeDir();
        if (!d.empty()) TUTORIAL_CH_DIR(d.c_str());
    }
};

// one instance per program; dynamic initialization runs before main()
inline ChdirToExeDir chdir_to_exe_dir{};

} // namespace tutorial_paths_detail
