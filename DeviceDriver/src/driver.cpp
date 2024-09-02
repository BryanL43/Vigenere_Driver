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
    // Free the memory allocated for the Request structure
    if (deviceObj->DeviceExtension != NULL) {
        ExFreePoolWithTag(deviceObj->DeviceExtension, 'ReqS');
        deviceObj->DeviceExtension = NULL;
    }

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

/*
* Sets the cipher job based on user specified command.
*
* @param deviceObj: The device object associated with the current driver.
                    This parameter is automatically provided by the I/O manager.
* @param irp: The I/O request packet representing the device_control request.
              This parameter is automatically provided by the I/O manager.
* @return NTSTATUS: STATUS_SUCCESS on success, STATUS_UNSUCCESSFUL on failure.
*/
NTSTATUS driver::device_control(PDEVICE_OBJECT deviceObj, PIRP irp) {
	UNREFERENCED_PARAMETER(deviceObj);

	debugPrint("[+] Device control called!\n");

	// Determines which code is passed through (holds ioctl codes)
	PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);

	// Access the request object sent from user mode
    auto request = reinterpret_cast<driver::Request*>(deviceObj->DeviceExtension);
	if (stack_irp == nullptr || request == nullptr) {
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_UNSUCCESSFUL;
	}

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
		break;
	}


	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = sizeof(driver::Request);

	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS driver::write(PDEVICE_OBJECT deviceObj, PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObj);

    debugPrint("[+] Write v1.14\n");

    PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);
    ULONG length = stack_irp->Parameters.Write.Length;

    // Validate the SystemBuffer pointer
    if (irp->AssociatedIrp.SystemBuffer == NULL) {
        debugPrint("[-] SystemBuffer is NULL!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    // Retrieve the Request structure from the device extension
    auto request = reinterpret_cast<driver::Request*>(deviceObj->DeviceExtension);

    if (request == NULL) {
        debugPrint("[-] Device extension is NULL!\n");
        irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_UNSUCCESSFUL;
    }

    // Check if the length is within bounds
    if (length > sizeof(request->message)) {
        length = sizeof(request->message);
    }

    // Copy the data from the SystemBuffer into the Request structure
    RtlCopyMemory(request->message, irp->AssociatedIrp.SystemBuffer, length);
    request->size = length;
    request->message[length] = '\0'; // Ensure null termination

    debugPrint("[+] Fired step 1\n");

    // Debug print the copied message
    debugPrint("[+] Copied message: ");
    debugPrint(request->message);
    debugPrint("\n");

    // Set the IRP's I/O status
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = length;

    // Complete the IRP
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


NTSTATUS driver::read(PDEVICE_OBJECT deviceObj, PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObj);

    debugPrint("[+] Read v1.02\n");

    PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);

    // Validate the SystemBuffer pointer
    if (irp->AssociatedIrp.SystemBuffer == NULL) {
        debugPrint("[-] SystemBuffer is NULL!\n");
        irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    auto request = reinterpret_cast<driver::Request*>(deviceObj->DeviceExtension);

    if (request == NULL) {
        debugPrint("[-] Device extension is NULL!\n");
        irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_UNSUCCESSFUL;
    }
    
    if (request->cipher == 1) {
        debugPrint("[+] Read encrypt should be fired!\n");
    }

    // Debug print the message
    debugPrint("[+] Read message: ");
    debugPrint(request->message);
    debugPrint("\n");

    ULONG messageLength = stack_irp->Parameters.Read.Length;

    // Ensure the length doesn't exceed the message size
    if (messageLength > sizeof(request->message)) {
        messageLength = sizeof(request->message) - 1;
    }
    request->message[messageLength] = '\0';

    // Copy the message to the SystemBuffer
    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, request->message, messageLength);

    // Set the IRP's I/O status
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = messageLength;

    // Complete the IRP
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}