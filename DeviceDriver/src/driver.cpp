#include "../headers/driver.h"

/*
* Prints to WinDbg.
* 
* @param text the const char* message.
*/
void debugPrint(PCSTR text) {
#ifndef DEBUG
	UNREFERENCED_PARAMETER(text);
#endif // Debug

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));
}

/*
* Remove the device from the system. Ideally through OSR Driver Loader.
* 
* @param DriverObject: The driver object associated with the current device.
*                      This parameter is automatically provided by the I/O manager.
*/
VOID driver::unload(PDRIVER_OBJECT DriverObject) {
    // Remove the symbolic link
	UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(L"\\DosDevices\\Vigenere_Driver");
	IoDeleteSymbolicLink(&symbolicLink);

	// Delete the device object
	IoDeleteDevice(DriverObject->DeviceObject);

	debugPrint("[+] Driver unloaded successfully.\n");
}

/*
* Initializes our data structure and opens the file descriptor.
*
* @param deviceObj: The device object associated with the current driver.
                    This parameter is automatically provided by the I/O manager.
* @param irp: The I/O request packet representing the create request.
              This parameter is automatically provided by the I/O manager.
* @return NTSTATUS: Either STATUS_SUCCESS for success
                    or STATUS_INSUFFICIENT_RESOURCES on failure.
*/
NTSTATUS driver::create(PDEVICE_OBJECT deviceObj, PIRP irp) {
    // Instantiate the device driver's data structure in the device extension.
    // ExAllocatePool2 is similar to vmalloc in C
    auto deviceExtension = 
        (driver::Request*) ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(driver::Request), 'ReqS');
    if (deviceExtension == NULL) {
        debugPrint("[-] Failed to instantiate device extension's data structure!\n");
        irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize the Request structure
    RtlZeroMemory(deviceExtension, sizeof(driver::Request));

    // Store the Request structure pointer in the device extension
    deviceObj->DeviceExtension = deviceExtension;

    // Set the IRP's I/O status
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

/*
* Free our data structure and completes the close operation.
*
* @param deviceObj: The device object associated with the current driver.
                    This parameter is automatically provided by the I/O manager.
* @param irp: The I/O request packet representing the close request.
              This parameter is automatically provided by the I/O manager.
* @return NTSTATUS: STATUS_SUCCESS on success.
*/
NTSTATUS driver::close(PDEVICE_OBJECT deviceObj, PIRP irp) {
    // Free the memory allocated for the Request data structure
    ExFreePoolWithTag(deviceObj->DeviceExtension, 'ReqS');
    deviceObj->DeviceExtension = NULL;

    // Set the IRP's I/O status
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

/*
* Sets the key and cipher job based on user specified command.
*
* @param deviceObj: The device object associated with the current driver.
                    This parameter is automatically provided by the I/O manager.
* @param irp: The I/O request packet representing the device_control request.
              This parameter is automatically provided by the I/O manager.
* @return NTSTATUS: STATUS_SUCCESS on success, or STATUS_INVALID_PARAMETER on invalid stack/request,
*                   or STATUS_INVALID_DEVICE_REQUEST on invalid driver code.
*/
NTSTATUS driver::device_control(PDEVICE_OBJECT deviceObj, PIRP irp) {
    // Acquire the stack I/O request packet which holds the ioctl codes
	PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);
    if (stack_irp == nullptr) {
        debugPrint("[-] Invalid IRP stack location!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    // Validate the SystemBuffer pointer
    if (irp->AssociatedIrp.SystemBuffer == nullptr) {
        debugPrint("[-] SystemBuffer is NULL!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

	// Acquire our data structure
    auto request = reinterpret_cast<driver::Request*>(deviceObj->DeviceExtension);
	if (request == nullptr) {
        debugPrint("[-] Failed to acquire our data structure!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

    // Retrieve the length of the key buffer
    ULONG length = stack_irp->Parameters.DeviceIoControl.InputBufferLength;

    // Check if the length is within bounds
    if (length > sizeof(request->key)) {
        length = sizeof(request->key);
    }

    // Copy the key data from the SystemBuffer into the data structure
    RtlCopyMemory(request->key, irp->AssociatedIrp.SystemBuffer, length);

	// Set the operation to encryption or decryption based on given command
	const ULONG controlCode = stack_irp->Parameters.DeviceIoControl.IoControlCode;
	switch (controlCode) {
		case driver::codes::encrypt:
			request->cipher = 1;
			break;
		case driver::codes::decrypt:
			request->cipher = 0;
			break;
	default:
		debugPrint("[-] Invalid driver code!\n");
        irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
	}

    // Set the IRP's I/O status
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = sizeof(driver::Request);
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

/*
* Writes the user's message to our data structure.
*
* @param deviceObj: The device object associated with the current driver.
                    This parameter is automatically provided by the I/O manager.
* @param irp: The I/O request packet representing the write request.
              This parameter is automatically provided by the I/O manager.
* @return NTSTATUS: STATUS_SUCCESS on success, or STATUS_INVALID_PARAMETER on invalid stack/request.
*/
NTSTATUS driver::write(PDEVICE_OBJECT deviceObj, PIRP irp) {
    // Acquire the stack I/O request packet which holds the ioctl codes
    PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);
    if (stack_irp == nullptr) {
        debugPrint("[-] Invalid IRP stack location!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    // Validate the SystemBuffer pointer
    if (irp->AssociatedIrp.SystemBuffer == nullptr) {
        debugPrint("[-] SystemBuffer is NULL!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    // Acquire our data structure
    auto request = reinterpret_cast<driver::Request*>(deviceObj->DeviceExtension);
    if (request == nullptr) {
        debugPrint("[-] Failed to acquire our data structure!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
    }

    // Retrieve the length of the write operation
    ULONG length = stack_irp->Parameters.Write.Length;

    // Check if the length is within bounds
    if (length > sizeof(request->message)) {
        length = sizeof(request->message);
    }

    // Copy the message data from the SystemBuffer into the data structure
    RtlCopyMemory(request->message, irp->AssociatedIrp.SystemBuffer, length);

    // Set the IRP's I/O status
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = length;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

/*
* Reads the encrypted/decrypted message from our data structure back to user.
*
* @param deviceObj: The device object associated with the current driver.
                    This parameter is automatically provided by the I/O manager.
* @param irp: The I/O request packet representing the read request.
              This parameter is automatically provided by the I/O manager.
* @return NTSTATUS: STATUS_SUCCESS on success, or STATUS_INVALID_PARAMETER on invalid stack/request.
*/
NTSTATUS driver::read(PDEVICE_OBJECT deviceObj, PIRP irp) {
    // Acquire the stack I/O request packet which holds the ioctl codes
    PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);
    if (stack_irp == nullptr) {
        debugPrint("[-] Invalid IRP stack location!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    // Validate the SystemBuffer pointer
    if (irp->AssociatedIrp.SystemBuffer == nullptr) {
        debugPrint("[-] SystemBuffer is NULL!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }
    
    // Acquire our data structure
    auto request = reinterpret_cast<driver::Request*>(deviceObj->DeviceExtension);
    if (request == nullptr) {
        debugPrint("[-] Failed to acquire our data structure!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    // Retrieve the length of the read operation
    ULONG messageLength = stack_irp->Parameters.Read.Length;

    // Check if the length is within bounds
    if (messageLength > sizeof(request->message)) {
        messageLength = sizeof(request->message);
    }

    // Copy the message to the SystemBuffer
    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, request->message, messageLength);

    // Set the IRP's I/O status
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = messageLength;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}