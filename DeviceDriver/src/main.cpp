#include <ntifs.h>

extern "C" {
	// Ensure our IOCTL driver is compatible with kdmapper
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName,
		PDRIVER_INITIALIZE InitializationFunction);

	// Implements read & write process memory in driver
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress,
		PEPROCESS TargetProcess, PVOID TargetAddress,
		SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode,
		PSIZE_T ReturnSize);
}

void debugPrint(PCSTR text); // Signature declaration for driver namespace

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

	NTSTATUS create(PDEVICE_OBJECT deviceObj, PIRP irp) {
		UNREFERENCED_PARAMETER(deviceObj);
		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}

	NTSTATUS close(PDEVICE_OBJECT deviceObj, PIRP irp) {
		UNREFERENCED_PARAMETER(deviceObj);
		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}

	NTSTATUS device_control(PDEVICE_OBJECT deviceObj, PIRP irp) {
		UNREFERENCED_PARAMETER(deviceObj);

		debugPrint("Device control called!\n");

		NTSTATUS status = STATUS_UNSUCCESSFUL;

		// Determines which code is passed through (holds attach, read, or write)
		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);

		// Access the request object sent from user mode
		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);
		if (stack_irp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status;
		}

		// target process we want to read/write memory to
		static PEPROCESS targetProcess = nullptr;
		const ULONG controlCode = stack_irp->Parameters.DeviceIoControl.IoControlCode;
		switch (controlCode) {
			case codes::attach:
				status = PsLookupProcessByProcessId(request->pid, &targetProcess);
				break;
			case codes::read:
				if (targetProcess != nullptr) {
					status = MmCopyVirtualMemory(targetProcess, request->target, PsGetCurrentProcess(),
						request->buffer, request->size, KernelMode, &request->returnSize);
				}
				break;
			case codes::write:
				if (targetProcess != nullptr) {
					status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer, targetProcess,
						request->target, request->size, KernelMode, &request->returnSize);
				}
				break;
			default:
				break;
		}

		irp->IoStatus.Status = status;
		irp->IoStatus.Information = sizeof(Request);

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return status;
	}
}

void debugPrint(PCSTR text) {
#ifndef DEBUG
	UNREFERENCED_PARAMETER(text);
#endif // Debug

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));
}

// Our "real" entry point
NTSTATUS driverMain(PDRIVER_OBJECT driverObj, PUNICODE_STRING registryPath) {
	UNREFERENCED_PARAMETER(registryPath); // Turn off registry path. Not referenced.

	UNICODE_STRING driverName = {}; // char* but Window's C++ version
	RtlInitUnicodeString(&driverName, L"\\Device\\Vigenere_Driver");

	PDEVICE_OBJECT deviceObj = nullptr;
	NTSTATUS status = IoCreateDevice(driverObj, 0, &driverName, FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObj);

	if (status != STATUS_SUCCESS) {
		debugPrint("[-] Failed to create device driver!\n");
		return status;
	}

	debugPrint("Device driver successfully created!\n");

	UNICODE_STRING symbolicLink = {};
	RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\Vigenere_Driver");

	status = IoCreateSymbolicLink(&symbolicLink, &driverName);
	if (status != STATUS_SUCCESS) {
		debugPrint("Failed to establish symbolic link!\n");
		return status;
	}

	debugPrint("Device symbolic link successfully established!\n");

	// Allow us to send small amounts of data between user/kernal mode
	SetFlag(deviceObj->Flags, DO_BUFFERED_IO);

	// Set the driver handlers to our functions with our logic
	driverObj->MajorFunction[IRP_MJ_CREATE] = driver::create;
	driverObj->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control;

	ClearFlag(deviceObj->Flags, DO_DEVICE_INITIALIZING);

	debugPrint("Driver initialized successfully!\n");

	return status;
}

// KdMapper will call this "entry point" but params will be null.
NTSTATUS DriverEntry() {
	debugPrint("HALLO WORLD!\n");

	UNICODE_STRING driverName = {}; // char* but Window's C++ version
	RtlInitUnicodeString(&driverName, L"\\Driver\\Vigenere_Driver");

	return IoCreateDriver(&driverName, &driverMain);
}