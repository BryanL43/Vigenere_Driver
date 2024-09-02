#ifndef DRIVER_H
#define DRIVER_H

#include <iostream>
#include <Windows.h>

namespace driver {
	namespace codes { // Holds the ioctl codes
		// Read process memory
		constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Write process memory
		constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Encrypt data
		constexpr ULONG encrypt =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Decrypt data
		constexpr ULONG decrypt =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	// Structure shared between user mode & kernal mode
	struct Request {
		int cipher; // Encrypt: 1; Decrypt: 0.
		char message[512];

		SIZE_T size;
		SIZE_T returnSize;
	};
}

#endif