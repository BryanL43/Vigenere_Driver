#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

static DWORD getPID(const wchar_t* processName) {
	DWORD pid = 0;

	HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snapShot == INVALID_HANDLE_VALUE) {
		return pid;
	}

	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Process32FirstW(snapShot, &entry) == TRUE) {
		// Check if the 1st handle is the one we want
		if (_wcsicmp(processName, entry.szExeFile) == 0) {
			pid = entry.th32ProcessID;
		} else {
			while (Process32NextW(snapShot, &entry) == TRUE) {
				if (_wcsicmp(processName, entry.szExeFile) == 0) {
					pid = entry.th32ProcessID;
					break;
				}
			}
		}
	}

	CloseHandle(snapShot);

	return pid;
}

static std::uintptr_t getModuleBase(const DWORD pid, const wchar_t* moduleName) {
	std::uintptr_t moduleBase = 0;

	HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (snapShot == INVALID_HANDLE_VALUE) {
		return pid;
	}

	MODULEENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Module32FirstW(snapShot, &entry) == TRUE) {
		if (wcsstr(moduleName, entry.szModule) != nullptr) {
			moduleBase = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
		} else {
			while (Module32NextW(snapShot, &entry) == TRUE) {
				if (wcsstr(moduleName, entry.szModule) != nullptr) {
					moduleBase = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
					break;
				}
			}
		}
	}

	CloseHandle(snapShot);

	return moduleBase;
}

namespace driver {
	namespace codes { // Holds the ioctl codes
		// Used to setup the driver
		constexpr ULONG attach =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Read process memory
		constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Write process memory
		constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	// Share between user mode & kernal mode
	struct Request {
		HANDLE pid;
		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T returnSize;
	};

	bool attachToProcess(HANDLE driverHandle, const DWORD pid) {
		Request r;
		r.pid = reinterpret_cast<HANDLE>(pid);

		return DeviceIoControl(driverHandle, codes::attach, &r, sizeof(r),
			&r, sizeof(r), nullptr, nullptr);
	}

	template<class T>
	T readMemory(HANDLE driverHandle, const std::uintptr_t address) {
		T temp = {};

		Request r;
		r.target = reinterpret_cast<PVOID>(address);
		r.buffer = &temp;
		r.size = sizeof(T);

		DeviceIoControl(driverHandle, codes::read, &r, sizeof(r),
			&r, sizeof(r), nullptr, nullptr);

		return temp;
	}

	template<class T>
	void writeMemory(HANDLE driverHandle, const std::uintptr_t address, const T& value) {
		Request r;
		r.target = reinterpret_cast<PVOID>(address);
		r.buffer = (PVOID) &value;
		r.size = sizeof(T);

		DeviceIoControl(driverHandle, codes::write, &r, sizeof(r),
			&r, sizeof(r), nullptr, nullptr);
	}
}

int main() {
	const DWORD pid = getPID(L"notepad.exe");
	if (pid == 0) {
		std::cout << "failed to find notepad!\n";
		std::cin.get();
		return 1;
	}

	const HANDLE driver = CreateFile(L"\\\\.\\Vigenere_Driver", GENERIC_READ, 0, nullptr, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (driver == INVALID_HANDLE_VALUE) {
		std::cout << "Failed to create our driver handle!\n";
		std::cin.get();
		return 1;
	}

	if (driver::attachToProcess(driver, pid) == true) {
		std::cout << "Attachment successful!\n";
	}

	CloseHandle(driver); // Prevent handle from being leaked to driver
	std::cin.get();

	return 0;
}