#ifndef DRIVER_H
#define DRIVER_H

#include <ntifs.h>
#include <ntstrsafe.h>
#include <ntddk.h>
#include <wdm.h>

#define BUFFER_SIZE 512

extern "C" {
	// Ensure our IOCTL driver is compatible with kdmapper
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName,
		PDRIVER_INITIALIZE InitializationFunction);;
}

void debugPrint(PCSTR text); // Signature declaration for driver namespace

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

	// File operation signatures
	NTSTATUS create(PDEVICE_OBJECT deviceObj, PIRP irp);
	NTSTATUS close(PDEVICE_OBJECT deviceObj, PIRP irp);
	NTSTATUS device_control(PDEVICE_OBJECT deviceObj, PIRP irp);
	NTSTATUS write(PDEVICE_OBJECT deviceObj, PIRP irp);
	NTSTATUS read(PDEVICE_OBJECT deviceObj, PIRP irp);

	VOID unload(PDRIVER_OBJECT DriverObject);
}

#endif