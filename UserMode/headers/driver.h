#ifndef DRIVER_H
#define DRIVER_H

#include <iostream>
#include <Windows.h>

#define BUFFER_SIZE 512

namespace driver {
	namespace codes { // Holds the ioctl codes
		// Encrypt data
		constexpr ULONG encrypt =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Decrypt data
		constexpr ULONG decrypt =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	// Data structure shared between user mode & kernal mode
	struct Request {
		int cipher; // Encrypt: 1; Decrypt: 0.
		char message[BUFFER_SIZE];
	};
}

#endif