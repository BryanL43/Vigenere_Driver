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
        return -1;
    }

    std::string message;
    std::string key;
    int cipherChoice;

    while (true) {
        // Get the user's message
        std::cout << "Enter the message (or type 'exit' to quit): ";
        std::getline(std::cin, message);

        // Exit the loop if the user types "exit"
        if (message == "exit") {
            break;
        }

        // Get the user's key
        std::cout << "Enter the key: ";
        std::getline(std::cin, key);

        // Get the cipher choice from the user
        std::cout << "Enter cipher mode (1 for encrypt, 0 for decrypt): ";
        std::cin >> cipherChoice;
        std::cin.ignore(); // Ignore new-line character

        // Write the message to the device driver
        DWORD bytesWritten;
        if (!WriteFile(driver, message.c_str(), message.size(), &bytesWritten, NULL)) {
            std::cerr << "Failed to write to the driver! Error: " << GetLastError() << std::endl;
            CloseHandle(driver);
            return -1;
        }

        // Set the encryption/decryption operation & assign the key
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(driver,
            cipherChoice == 1 ? driver::codes::encrypt : driver::codes::decrypt,
            (LPVOID) key.c_str(), sizeof(key), nullptr, 0, &bytesReturned, nullptr);
        if (!result) {
            std::cerr << "DeviceIoControl failed. Error: " << GetLastError() << std::endl;
            CloseHandle(driver);
            return -1;
        }

        // Read the device driver's response
        char readBuffer[sizeof(message)];
        DWORD bytesRead = 0;
        if (!ReadFile(driver, readBuffer, sizeof(readBuffer) - 1, &bytesRead, NULL)) {
            std::cerr << "Failed to read from the driver! Error: " << GetLastError() << std::endl;
            CloseHandle(driver);
            return -1;
        }

        // Output the result
        std::cout << "Result: " << readBuffer << "\n";
    }

    CloseHandle(driver);
    return 0;
}