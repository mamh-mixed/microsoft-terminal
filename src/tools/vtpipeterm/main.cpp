// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <Windows.h>

#define CONPTY_IMPEXP
#include <conpty-static.h>

int wmain(int argc, WCHAR* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    const auto outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    HPCON hPC = nullptr;
    HANDLE stdoutRx, stdinTx;
    {
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex{ .cbSize = sizeof(csbiex) };
        GetConsoleScreenBufferInfoEx(outputHandle, &csbiex);

        HANDLE stdoutTx, stdinRx;
        CreatePipe(&stdoutRx, &stdoutTx, nullptr, 0);
        CreatePipe(&stdinRx, &stdinTx, nullptr, 0);

        const SHORT w = csbiex.srWindow.Right - csbiex.srWindow.Left + 1;
        const SHORT h = csbiex.srWindow.Bottom - csbiex.srWindow.Top + 1;
        ConptyCreatePseudoConsole({ w, h }, stdinRx, stdoutTx, 0, &hPC);
    }

    PROCESS_INFORMATION pi;
    {
        wchar_t commandline[MAX_PATH] = LR"(C:\Windows\System32\cmd.exe)";

        STARTUPINFOEX siEx{};
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        char attrList[64];
        size_t size = sizeof(attrList);
        siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&attrList[0]);
        InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size);
        UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), nullptr, nullptr);

        CreateProcessW(
            nullptr,
            commandline,
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            false, // bInheritHandles
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            nullptr, // lpEnvironment
            nullptr,
            &siEx.StartupInfo, // lpStartupInfo
            &pi // lpProcessInformation
        );
    }

    ConptyReleasePseudoConsole(hPC);

    CreateThread(
        nullptr, 0, [](HANDLE stdinTx) -> DWORD {
            char buf[4096];
            DWORD read;
            while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf, sizeof(buf), &read, nullptr) && read)
            {
                WriteFile(stdinTx, buf, read, nullptr, nullptr);
            }
            return 0;
        },
        stdinTx,
        0,
        nullptr);

    DWORD previousOutputMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
    GetConsoleMode(outputHandle, &previousOutputMode);
    SetConsoleMode(outputHandle, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);

    const auto previousInputCP = GetConsoleCP();
    const auto previousOutputCP = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    char buf[4096];
    DWORD read;
    while (ReadFile(stdoutRx, buf, sizeof(buf), &read, nullptr) && read)
    {
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, read, nullptr, nullptr);
    }

    SetConsoleMode(outputHandle, previousOutputMode);
    SetConsoleCP(previousInputCP);
    SetConsoleOutputCP(previousOutputCP);
    return 0;
}
