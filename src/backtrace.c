#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#elif defined(__MINGW32__)
#include <windows.h>
#define DBGHELP_TRANSLATE_TCHAR
#include <DbgHelp.h>
#include <imagehlp.h>
#endif

#include "backtrace.h"
#include "log.h"

void backtrace_print() {
#if defined(__linux__) || defined(__APPLE__)
    void *array[50];
    int size;

    size = backtrace(array, 50);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
#elif defined(__MINGW32__)
    CONTEXT context = { 0 };
    STACKFRAME frame = { 0 };

    const HANDLE hProcess = GetCurrentProcess();
    const HANDLE hThread = GetCurrentThread();

    context.ContextFlags = CONTEXT_FULL;

    RtlCaptureContext(&context);
    SymInitialize(GetCurrentProcess(), 0, true);
    SymSetOptions(SymGetOptions() | SYMOPT_UNDNAME);

    DWORD image;
    STACKFRAME64 stackframe;
    ZeroMemory(&stackframe, sizeof(STACKFRAME64));

#ifdef _M_IX86
    image = IMAGE_FILE_MACHINE_I386;
    stackframe.AddrPC.Offset = context.Eip;
    stackframe.AddrPC.Mode = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.Ebp;
    stackframe.AddrFrame.Mode = AddrModeFlat;
    stackframe.AddrStack.Offset = context.Esp;
    stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    image = IMAGE_FILE_MACHINE_AMD64;
    stackframe.AddrPC.Offset = context.Rip;
    stackframe.AddrPC.Mode = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.Rsp;
    stackframe.AddrFrame.Mode = AddrModeFlat;
    stackframe.AddrStack.Offset = context.Rsp;
    stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    image = IMAGE_FILE_MACHINE_IA64;
    stackframe.AddrPC.Offset = context.StIIP;
    stackframe.AddrPC.Mode = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.IntSp;
    stackframe.AddrFrame.Mode = AddrModeFlat;
    stackframe.AddrBStore.Offset = context.RsBSP;
    stackframe.AddrBStore.Mode = AddrModeFlat;
    stackframe.AddrStack.Offset = context.IntSp;
    stackframe.AddrStack.Mode = AddrModeFlat;
#endif

    int stackframe_index = 0;
    while (StackWalk64(
            image,
            hProcess,
            hThread,
            &stackframe,
            &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL)) {
        DWORD64 displacement = 0;
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];

        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        printf("[%i][0x%016llx] ", stackframe_index, frame.AddrPC.Offset);
        if (SymFromAddr(hProcess, stackframe.AddrPC.Offset, &displacement, symbol)) {
            printf("%s\n", symbol->Name);
        } else {
            printf("???\n");
        }

        stackframe_index++;
    }

    SymCleanup(hProcess);
#else
#error Platform not supported
#endif
}
