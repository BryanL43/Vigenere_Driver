#include "../headers/cipher.h"

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
 * Repeats the key to the length of the message to be encrypted/decrypted.
 * i.e. "This is a test message" with "key" : "key" -> "keykeykeykeykeykeykeyk"
 * 
 * @param key the key to be resolved.
 * @param msgLen the length of the message to be encrypted/decrypted.
 * @return the resolved key or nullptr on failure.
*/
char* resolveKey(char* key, int msgLen) {
    // Instantiate resolved key buffer
    char* resolvedKey =
        (char*) ExAllocatePool2(POOL_FLAG_NON_PAGED, msgLen + 1, 'kyRS');
    if (resolvedKey == nullptr) {
        return nullptr;
    }

    // Iterate through the length of the message to create a repeated key
    // that matches the length of the message.
    int j = 0;
    for (int i = 0; i < msgLen; i++) {
        if (j == strlen(key)) {
            j = 0;
        }
        resolvedKey[i] = key[j];
        j++;
    }
    resolvedKey[msgLen] = '\0';

    return resolvedKey;
}

/*
* Encrypts a singular alphabetical character using Vigenère cipher.
* 
* @param c the character to be encrypted.
* @param key the key character that determines the shift amount.
* @return the encrypted character.
*/
char encryptChar(char c, char key) {
    if (c >= 'a' && c <= 'z') { // case: lowercase letters

        // If c is lowercase, convert key to lowercase if it's not already
        if (key >= 'A' && key <= 'Z') {
            key = key + ('a' - 'A');
        }

        return ((c - 'a' + (key - 'a')) % 26) + 'a';
    }
    else if (c >= 'A' && c <= 'Z') { // case: uppercase letters

        // If c is uppercase, convert key to uppercase if it's not already
        if (key >= 'a' && key <= 'z') {
            key = key - ('a' - 'A');
        }

        return ((c - 'A' + (key - 'A')) % 26) + 'A';
    }
    else {
        return c;
    }
}

/*
* Decrypts a singular alphabetical character using Vigenère cipher.
* 
* @param c the character to be decrypted.
* @param key the key character that determines the shift amount.
* @return the decrypted character.
*/
char decryptChar(char c, char key) {
    if (c >= 'a' && c <= 'z') { // case: lowercase letter

        // If c is lowercase, convert key to lowercase if it's not already
        if (key >= 'A' && key <= 'Z') {
            key = key + ('a' - 'A');
        }

        return ((c - 'a' - (key - 'a') + 26) % 26) + 'a';
    }
    else if (c >= 'A' && c <= 'Z') { // case: uppercase letter

        // If c is uppercase, convert key to uppercase if it's not already
        if (key >= 'a' && key <= 'z') {
            key = key - ('a' - 'A');
        }

        return ((c - 'A' - (key - 'A') + 26) % 26) + 'A';
    }
    else {
        return c;
    }
}

/*
* Encrypts the message with its associated key.
* 
* @param text: the message to be encrypted.
* @param key: the key used to encrypt the message.
* @return STATUS_SUCCESS on success, STATUS_INVALID_PARAMETER
                          or STATUS_INSUFFICIENT_RESOURCES on errors.
*/
NTSTATUS encrypt(char* message, char* key) {
    if (message == nullptr || key == nullptr) { // Validate parameters
        debugPrint("[-] Null pointer passed to encrypt function!\n");
        return STATUS_INVALID_PARAMETER;
    }

    // Cannot acquire length of message conventionally. Have to work around.
    int msgLen = BUFFER_SIZE;

    // Repeats the key to ensure that key length matches the message,
    // which is necessary for Vigenère cipher.
    char* resolvedKey = resolveKey(key, msgLen);
    if (resolvedKey == nullptr) {
        debugPrint("[-] Failed to allocate memory for resolvedKey!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Iterate through the message, encrypting one letter at a time
    // and excluding any non-alphabetical characters.
    int j = 0;
    for (int i = 0; message[i] != '\0'; i++) {
        char buffer[2] = { message[i], '\0' };

        if ((buffer[0] >= 'A' && buffer[0] <= 'Z') || (buffer[0] >= 'a' && buffer[0] <= 'z')) {
            char encryptedChar = encryptChar(buffer[0], resolvedKey[j]);
            message[i] = encryptedChar;
            j++;
        }
        else { // Keep non-alphabetical characters unchanged
            message[i] = buffer[0];
        }
    }

    ExFreePool(resolvedKey); // free resolvedKey
    return STATUS_SUCCESS;
}

/*
* Decrypts the message with its associated key.
*
* @param text: the message to be decrypted.
* @param key: the key used to decrypt the message.
* @return STATUS_SUCCESS on success, STATUS_INVALID_PARAMETER
                          or STATUS_INSUFFICIENT_RESOURCES on errors.
*/
NTSTATUS decrypt(char* message, char* key) {
    if (message == nullptr || key == nullptr) { // Validate parameters
        debugPrint("[-] Null pointer passed to decrypt function!\n");
        return STATUS_INVALID_PARAMETER;
    }

    // Cannot acquire length of message conventionally. Have to work around.
    int msgLen = BUFFER_SIZE;

    // Repeats the key to ensure that key length matches the message,
    // which is necessary for Vigenère cipher.
    char* resolvedKey = resolveKey(key, msgLen);
    if (resolvedKey == nullptr) {
        debugPrint("[-] Failed to allocate memory for resolvedKey!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Iterate through the message, decrypting one letter at a time
    // and excluding any non-alphabetical characters.
    int j = 0;
    for (int i = 0; message[i] != '\0'; i++) {
        char buffer[2] = { message[i], '\0' };

        if ((buffer[0] >= 'A' && buffer[0] <= 'Z') || (buffer[0] >= 'a' && buffer[0] <= 'z')) {
            char decryptedChar = decryptChar(buffer[0], resolvedKey[j]);
            message[i] = decryptedChar;
            j++;
        }
        else { // Keep non-alphabetical characters unchanged
            message[i] = buffer[0];
        }
    }

    ExFreePool(resolvedKey); // free resolvedKey
    return STATUS_SUCCESS;
}