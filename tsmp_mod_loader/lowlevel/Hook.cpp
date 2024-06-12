#include "..\Common.h"

void AllowWriteToMemory(void* memory, u32 size)
{
	DWORD oldProtect = 0;
	bool res = VirtualProtect(memory, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	uniassert(res, "Failed to change memory rights");
}

string dwordtostr(u32 input)
{
	union u32represent
	{
		char ch[4];
		u32 u;
	};

	u32represent rep;
	rep.u = input;

	string res;
	res += rep.ch[0];
	res += rep.ch[1];
	res += rep.ch[2];
	res += rep.ch[3];

	return res;
}

string chr(u32 input)
{
	string str;
	str += static_cast<char>(input);
	return str;
}

// Заменяет srcFunc на newFunc, переписывая bytesToRewrite байт, возвращает указатель для вызова srcFunc 
void *HookFunction(void *srcFunc, void *newFunc, u32 bytesToRewrite)
{
	const string jmpOpcode = chr(0xe9);
	const u32 jmpCodeSize = jmpOpcode.size() + sizeof(u32);

	// В новый блок памяти помещаем первые байты функции srcFunc и jmp на оставшиеся
	u32 newBlockSize = bytesToRewrite + jmpCodeSize;
	void *newCodeBlock = malloc(newBlockSize);
	AllowWriteToMemory(newCodeBlock, newBlockSize);

	memcpy_s(newCodeBlock, bytesToRewrite, srcFunc, bytesToRewrite);
	void *jmpAddr = reinterpret_cast<void*>(reinterpret_cast<u32>(newCodeBlock) + bytesToRewrite);
	const u32 jmpOffset = reinterpret_cast<u32>(srcFunc) - reinterpret_cast<u32>(jmpAddr) - jmpCodeSize + bytesToRewrite;
	string jmpToRemainingSrc = jmpOpcode + dwordtostr(jmpOffset);
	memcpy_s(jmpAddr, bytesToRewrite, jmpToRemainingSrc.data(), bytesToRewrite);

	// Код прыжка на новую функцию
	const u32 jmpOffset2 = reinterpret_cast<u32>(newFunc) - reinterpret_cast<u32>(srcFunc) - jmpCodeSize;

	string jmpToNewCode = jmpOpcode + dwordtostr(jmpOffset2);
	uniassert(bytesToRewrite >= jmpToNewCode.size(), "Hook size mismatch!");

	while (jmpToNewCode.size() < bytesToRewrite)
		jmpToNewCode += chr(0x90); // nop

	AllowWriteToMemory(srcFunc, bytesToRewrite);
	memcpy_s(srcFunc, bytesToRewrite, jmpToNewCode.data(), bytesToRewrite);

	return newCodeBlock;
}

// Возвращает на место исходную функцию
void UnHookFunction(void* srcFunc, void* newFuncBlock, u32 bytesToRewrite)
{
	memcpy_s(srcFunc, bytesToRewrite, newFuncBlock, bytesToRewrite);
}
