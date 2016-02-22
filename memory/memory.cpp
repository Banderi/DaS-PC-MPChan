// memory.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <locale>
#include <Windows.h>
#include <codecvt>
#include <vector>
#include <TlHelp32.h>
#include <Psapi.h>

using namespace std;

DWORD FindProcessId(string processName2)
{
	//setup converter
	typedef codecvt_utf8<wchar_t> convert_type;
	wstring_convert<convert_type, wchar_t> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	const wstring processName = converter.from_bytes(processName2);

	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(processInfo);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (processesSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	Process32First(processesSnapshot, &processInfo);
	if (!processName.compare(processInfo.szExeFile))
	{
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo))
	{
		if (!processName.compare(processInfo.szExeFile))
		{
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}

HANDLE GetProcessByName(const char* name)
{
	return OpenProcess(PROCESS_ALL_ACCESS, FALSE, FindProcessId(name));
}

vector<byte> readMemory(HANDLE hProcess, DWORD address, int length) {
	byte *buffer = new byte[length];
	SIZE_T bytesRead;
	vector<byte> buffer2;
	ReadProcessMemory(hProcess, (void *)address, buffer, length, &bytesRead);
	for (int i = 0; i < length; i++)
	{
		buffer2.push_back(buffer[i]);
	}
	return buffer2;
}

vector<byte> readMemory(HANDLE hProcess, DWORD *address, int length) {
	byte *buffer = new byte[length];
	SIZE_T bytesRead;
	vector<byte> buffer2;
	ReadProcessMemory(hProcess, (void *)address, buffer, length, &bytesRead);
	for (int i = 0; i < length; i++)
	{
		buffer2.push_back(buffer[i]);
	}
	return buffer2;
}

DWORD dwGetModuleBaseAddress(DWORD dwProcessIdentifier, TCHAR *lpszModuleName)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessIdentifier);
	DWORD dwModuleBaseAddress = 0;
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 ModuleEntry32 = { 0 };
		ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
		if (Module32First(hSnapshot, &ModuleEntry32))
		{
			do
			{
				if (_tcscmp(ModuleEntry32.szModule, lpszModuleName) == 0)
				{
					dwModuleBaseAddress = (DWORD)ModuleEntry32.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnapshot, &ModuleEntry32));
		}
		CloseHandle(hSnapshot);
	}
	return dwModuleBaseAddress;
}

union MERGE{
	BYTE byte[4];
	DWORD num;
};

DWORD bufferToPointer(vector<byte> buffer)
{
	
	BYTE b[4];
	b[0] = buffer.at(0);
	b[1] = buffer.at(1);
	b[2] = buffer.at(2);
	b[3] = buffer.at(3);
	MERGE m;

	memcpy(m.byte, b, 4);

	return m.num;
}

DWORD concatenatePointer(HANDLE hProcess, vector<DWORD> offset, DWORD baseaddress)
{
	vector<byte> buffer = readMemory(hProcess, baseaddress, 4);
	DWORD pointer = bufferToPointer(buffer);;

	for (int i = 0; i < offset.size()-1; i++)
	{		
		buffer = readMemory(hProcess, pointer + offset[i], 4);
		pointer = bufferToPointer(buffer);
	}

	return bufferToPointer(buffer) + offset[offset.size()-1];
}

struct node
{
	DWORD m_baseAddress;
	int m_nodeNumber;
	string m_id64;
	string m_name;
};

node nullNode = { NULL, NULL, NULL, NULL };

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE  hProcess = GetProcessByName("DARKSOULS.exe");

	//DWORD *address = new DWORD;

	//int lpBaseAddress = 0x0395b99c; //0x0395ba14;
	DWORD lpBaseAddress = dwGetModuleBaseAddress(FindProcessId("DARKSOULS.exe"), _T("DARKSOULS.exe"));
	vector<byte> buffer;

	//

	buffer = readMemory(hProcess, lpBaseAddress, 4);

	cout << "base address: 0x";
	printf("%08X", lpBaseAddress);
	cout << " (";
	for (int i = 0; i < 4; i++)
		printf("%02X", buffer.at(3-i));
	cout << ")" << endl;
	cout << endl;

	// CONCATENATE

	cout << "nodes base address: 0x";
	printf("%08X\n", bufferToPointer(readMemory(hProcess, concatenatePointer(hProcess, { 0xa0, 0x0, 0x8, 0x40, 0x9c }, lpBaseAddress + 0x00F63BB0), 4)));
	cout << endl;
	cout << "nodes:" << endl;

	buffer = readMemory(hProcess, concatenatePointer(hProcess, { 0xa0, 0x0, 0x8, 0x40, 0x114 }, lpBaseAddress + 0x00F63BB0), 16);

	for (int i = 0; i < 16; i++)
		printf(" %c ", buffer.at(i));
	cout << endl;
	for (int i = 0; i < 16; i++)
		printf("%02X ", buffer.at(i));
	for (int i = 0; i < 16; i++)
		printf("%c", buffer.at(i));

	buffer = readMemory(hProcess, lpBaseAddress, 8);

	cout << endl;
	cout << endl;
	system("pause");
	//getchar();
	return 0;
}

