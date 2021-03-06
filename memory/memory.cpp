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

void cls()
{
	HANDLE console = ::GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	::GetConsoleScreenBufferInfo(console, &csbi);
	COORD origin = { 0, 0 };
	DWORD written;
	::FillConsoleOutputCharacterA(console, ' ', csbi.dwSize.X * csbi.dwSize.Y,
		origin, &written);
	::FillConsoleOutputAttribute(console, csbi.wAttributes, csbi.dwSize.X * csbi.dwSize.Y,
		origin, &written);
	::SetConsoleCursorPosition(console, origin);
}

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

vector<byte> readMemory(HANDLE hProcess, DWORD address, int length)
{
	byte *buffer = new byte[length];
	SIZE_T bytesRead;
	vector<byte> buffer2;
	ReadProcessMemory(hProcess, (void *)address, buffer, length, NULL);
	for (int i = 0; i < length; i++)
	{
		buffer2.push_back(buffer[i]);
	}
	return buffer2;
}

void writeMemory(HANDLE hProcess, DWORD address, vector<byte> value)
{
	byte *buffer = new byte[];
	for (int i = 0; i < value.size(); i++)
	{
		buffer[i] = value.at(i);
	}
	WriteProcessMemory(hProcess, (void *)address, buffer, value.size(), NULL);
}

union MERGE
{
	BYTE byte[];
	DWORD num;
	char str[];
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

string bufferToString(vector<byte> buffer)
{
	string str = "";

	for (int i = 0; i < buffer.size(); i++)
	{
		if (buffer.at(i) != 0x00)
			str += char(buffer.at(i));
	}

	return str;
}

string bufferToUnicode(vector<byte> buffer)
{
	string str = "";

	for (int i = 0; i < buffer.size(); i++)
	{
		if (i % 2 == 0 && buffer.at(i) != 0x00)
			str += char(buffer.at(i));
	}

	return str;
}

vector<byte> stringToBuffer(string str)
{
	vector<byte> buffer;

	for (int i = 0; i < str.length(); i++)
	{
		buffer.push_back(str[i]);
	}

	return buffer;
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
	bool m_active;
	DWORD m_baseAddress;
	int m_nodeNumber;
	string m_id64;
	string m_id64_old;
	string m_name;
	node(bool l, DWORD a, int n, string i, string m) : m_active(l), m_baseAddress(a), m_nodeNumber(n), m_id64(i), m_name(m)
	{
		m_id64_old = "";
		m_id64_old = m_id64;
	}
};

node nullNode = { 0, NULL, NULL, "", "" };
vector<node> nodes;
int nodesCount = 32; // default: 20 - changes with DSFix

void prepareNodes();
void readNodes();
void printNodes();
void renameNode(int node);
void renameAllNodes();
void restoreNode(int node);
void restoreAllNodes();

HANDLE hProcess; // handle to the Dark Souls process
DWORD processCode;
DWORD lpBaseAddress; // process entry address
DWORD lpNodesAddress; // nodes base address
vector<byte> buffer;


int _tmain(int argc, _TCHAR* argv[])
{
	//atexit(exiting);

	hProcess = GetProcessByName("DARKSOULS.exe");
	lpBaseAddress = dwGetModuleBaseAddress(FindProcessId("DARKSOULS.exe"), _T("DARKSOULS.exe"));
	lpNodesAddress = bufferToPointer(readMemory(hProcess, concatenatePointer(hProcess, { 0x2c, 0xc, 0x8, 0x18 }, lpBaseAddress + 0x00F62D34), 4));

	prepareNodes();

	while (GetExitCodeProcess(hProcess, &processCode) && processCode == STILL_ACTIVE)
	{
		cout << endl;
		cout << " process base address: 0x";
		printf("%08X", lpBaseAddress);
		cout << endl;

		cout << " nodes base address?: 0x";
		printf("%08X\n", lpNodesAddress);
		cout << endl;
		cout << " nodes: " << bufferToPointer(readMemory(hProcess, lpNodesAddress - 0x40, 4)) << "/" << nodesCount << endl;

		readNodes();
		printNodes();
		Sleep(1000);
		cls();
	}
	
	return 0;
}

void prepareNodes()
{
	for (int i = 0; i < nodesCount; i++)
	{
		nodes.push_back(nullNode);
		nodes.at(i).m_nodeNumber = i;
	}
}

void readNodes()
{
	for (int i = 0; i < nodesCount; i++)
	{
		DWORD pointer = lpNodesAddress + DWORD(i * 0xb0); // node base pointer
		string id = bufferToString(readMemory(hProcess, pointer + 0x54, 16)); // steamid64
		string name = bufferToUnicode(readMemory(hProcess, concatenatePointer(hProcess, { 0x14, 0x30 }, pointer + 0x24), 64)); // steam name

		nodes.at(i).m_baseAddress = pointer;

		if (id.size() != 0 && id.at(0) != 0x00)
		{
			nodes.at(i).m_active = 1;

			if (nodes.at(i).m_name != name)
			{				
				nodes.at(i).m_id64 = id;
				nodes.at(i).m_id64_old = id;
				nodes.at(i).m_name = name;
				renameNode(i);
			}
		}
		else
		{
			nodes.at(i).m_active = 0;
			nodes.at(i).m_id64 = "";
			nodes.at(i).m_id64_old = "";
			nodes.at(i).m_name = "";
		}
	}
	
	return;
}

void printNodes()
{
	cout << "------------------" << endl;
	for (int i = 0; i < nodes.size(); i++)
	{
		cout << " ";
		printf("%2i", nodes.at(i).m_nodeNumber);
		printf(" (0x%08X): ", nodes.at(i).m_baseAddress);
		if (nodes.at(i).m_active)
		{
			cout << nodes.at(i).m_id64 << " (" << nodes.at(i).m_id64_old << ") - " << nodes.at(i).m_name;
		}
		else
		{
			cout << "N/A";
		}
		
		cout << endl;	
	}
}

void renameNode(int node)
{
	string str = nodes.at(node).m_name;
	str.resize((size_t)16, (char)' ');
	writeMemory(hProcess, nodes.at(node).m_baseAddress + 0x54, stringToBuffer(str));
	nodes.at(node).m_id64 = str;
}

void renameAllNodes()
{
	for (int i = 0; i < nodesCount; i++)
	{
		if (nodes.at(i).m_active)
			renameNode(i);
	}
}

void restoreNode(int node)
{
	writeMemory(hProcess, nodes.at(node).m_baseAddress + 0x54, stringToBuffer(nodes.at(node).m_id64_old));
	nodes.at(node).m_id64 = nodes.at(node).m_id64_old;
}

void restoreAllNodes()
{
	for (int i = 0; i < nodesCount; i++)
	{
		if (nodes.at(i).m_active)
			restoreNode(i);
	}
}