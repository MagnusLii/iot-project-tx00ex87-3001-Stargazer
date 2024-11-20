#pragma once

#ifndef ENABLE_DEBUG
#define DEBUG(...) ((void)0)
#else
#include <iostream>

namespace dbg {
template <typename... Args> void print(const char *file, int line, Args... args) {
    std::clog << "(DEBUG) [" << file << ":" << line << "] ";
    ((std::clog << args << " "), ...);
    std::clog << std::endl;
}
} // namespace dbg

#define DEBUG(...) dbg::print(__FILE__, __LINE__, __VA_ARGS__)
#endif
