#ifndef CIPHER_H
#define CIPHER_H

#include <ntifs.h>

#define BUFFER_SIZE 512

void debugPrint(PCSTR text);

// Cipher operation signatures
char* resolveKey(char* key, int msgLen);
char encryptChar(char c, char key);
char decryptChar(char c, char key);
NTSTATUS encrypt(char* message, char* key);
NTSTATUS decrypt(char* message, char* key);

#endif