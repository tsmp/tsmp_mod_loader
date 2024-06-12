#pragma once
#include <string>
#include <mutex>
#include <vector>
#include <Windows.h>

using std::string;
using std::vector;
using u32 = unsigned int;

void Msg(const char* format, ...);
void uniassert(bool cond, const string& descr);

void* HookFunction(void* srcFunc, void* newFunc, u32 bytesToRewrite);
void UnHookFunction(void* srcFunc, void* newFuncBlock, u32 bytesToRewrite);

typedef char string1024[1024];
