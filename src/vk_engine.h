#pragma once

#include <cstdio>


inline void PrintHelloWorld() noexcept
{
    fprintf_s(stdout, "Hello World!\n");
}