// Memory and string functions for Cemu patches, implemented by the patch runtime

#ifndef _CEMU_PATCH_STRING_H
#define _CEMU_PATCH_STRING_H

#include <_cemu_patch_libc.h>
#include <stddef.h>

_CEMU_PATCH_BEGIN_C

void* memcpy(void* __restrict dst, const void* __restrict src, size_t size) _CEMU_PATCH_NOEXCEPT;
void* memmove(void* dst, const void* src, size_t size) _CEMU_PATCH_NOEXCEPT;
void* memset(void* dst, int value, size_t size) _CEMU_PATCH_NOEXCEPT;
int memcmp(const void* a, const void* b, size_t size) _CEMU_PATCH_NOEXCEPT;
void* memchr(const void* ptr, int value, size_t size) _CEMU_PATCH_NOEXCEPT;
size_t strlen(const char* str) _CEMU_PATCH_NOEXCEPT;
size_t strnlen(const char* str, size_t maxSize) _CEMU_PATCH_NOEXCEPT;
int strcmp(const char* a, const char* b) _CEMU_PATCH_NOEXCEPT;
int strncmp(const char* a, const char* b, size_t size) _CEMU_PATCH_NOEXCEPT;
int strcoll(const char* a, const char* b) _CEMU_PATCH_NOEXCEPT;
size_t strxfrm(char* __restrict dst, const char* __restrict src, size_t size) _CEMU_PATCH_NOEXCEPT;
char* strchr(const char* str, int c) _CEMU_PATCH_NOEXCEPT;
char* strrchr(const char* str, int c) _CEMU_PATCH_NOEXCEPT;
char* strstr(const char* str, const char* find) _CEMU_PATCH_NOEXCEPT;
char* strpbrk(const char* str, const char* accept) _CEMU_PATCH_NOEXCEPT;
size_t strspn(const char* str, const char* accept) _CEMU_PATCH_NOEXCEPT;
size_t strcspn(const char* str, const char* reject) _CEMU_PATCH_NOEXCEPT;
char* strtok(char* __restrict str, const char* __restrict separators) _CEMU_PATCH_NOEXCEPT;
char* strtok_r(char* __restrict str, const char* __restrict separators, char** __restrict state) _CEMU_PATCH_NOEXCEPT;
char* strcpy(char* __restrict dst, const char* __restrict src) _CEMU_PATCH_NOEXCEPT;
char* strncpy(char* __restrict dst, const char* __restrict src, size_t size) _CEMU_PATCH_NOEXCEPT;
char* strcat(char* __restrict dst, const char* __restrict src) _CEMU_PATCH_NOEXCEPT;
char* strncat(char* __restrict dst, const char* __restrict src, size_t size) _CEMU_PATCH_NOEXCEPT;
// allocated with malloc
char* strdup(const char* str) _CEMU_PATCH_NOEXCEPT;
char* strndup(const char* str, size_t maxSize) _CEMU_PATCH_NOEXCEPT;
char* strerror(int error) _CEMU_PATCH_NOEXCEPT;

_CEMU_PATCH_END_C

#endif
