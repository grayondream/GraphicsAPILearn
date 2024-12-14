#include "ConsoleDebugger.hpp"
#include "Base/DXH.hpp"
#include <cstdio>
ConsoleDebugger::ConsoleDebugger(){
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	AllocConsole();
	auto _ = freopen("CONOUT$", "w", stdout);
#endif
}

ConsoleDebugger::~ConsoleDebugger(){
#ifdef _DEBUG
	FreeConsole();
	system("pause");
#endif
}
