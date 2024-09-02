#include <windows.h>
#include <iostream>
#include <string>

#include "../headers/driver.h"

int main() {
    // Open a handle to the driver
    const HANDLE driver = CreateFile(L"\\\\.\\Vigenere_Driver", GENERIC_WRITE | GENERIC_READ, 0,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (driver == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create our driver handle! Error: " << GetLastError() << std::endl;
        std::cin.get();
        return -1;
    }

    char message[] = "Hello, my name is Bryan Lee. I Like exploding my computer.";

    DWORD bytesWritten;
    if (!WriteFile(driver, message, sizeof(message) - 1, &bytesWritten, NULL)) {
        std::cerr << "Failed to write to the driver! Error: " << GetLastError() << std::endl;
    } else {
        std::cout << "Bytes written: " << bytesWritten << "\n";
    }

    DWORD bytesReturned;
    BOOL result = DeviceIoControl(driver, driver::codes::encrypt, nullptr, 0,
        nullptr, 0, &bytesReturned, nullptr);
    if (!result) {
        std::cerr << "DeviceIoControl failed. Error: " << GetLastError() << std::endl;
        CloseHandle(driver);
        return -1;
    }

    char readBuffer[sizeof(message)] = { 0 };
    DWORD bytesRead = 0;

    // Read the response from the driver
    if (!ReadFile(driver, readBuffer, sizeof(readBuffer), &bytesRead, NULL)) {
        std::cerr << "Failed to read from the driver! Error: " << GetLastError() << std::endl;
    } else {
        std::cout << "Bytes read: " << bytesRead << "\n";
        std::cout << "Read message: " << readBuffer << "\n";
    }

    CloseHandle(driver);

    std::cin.get();
    return 0;
}