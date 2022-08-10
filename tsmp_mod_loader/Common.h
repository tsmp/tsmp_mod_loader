#pragma once
#include <string>
#include <mutex>
#include <vector>
#include <Windows.h>

using std::string;
using std::vector;
using u32 = unsigned int;

void Msg(const char* format, ...);

typedef char string1024[1024];
