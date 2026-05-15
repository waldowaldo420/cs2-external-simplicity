#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <type_traits>

typedef LONG NTSTATUS;
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING_, * PUNICODE_STRING_;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING_ ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES_, * POBJECT_ATTRIBUTES_;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID_, * PCLIENT_ID_;

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA_, * PPEB_LDR_DATA_;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING_ FullDllName;
    UNICODE_STRING_ BaseDllName;
} LDR_DATA_TABLE_ENTRY_, * PLDR_DATA_TABLE_ENTRY_;

typedef struct _PEB {
    BYTE  Reserved1[2];
    BYTE  BeingDebugged;
    BYTE  Reserved2[1];
    PVOID Reserved3[2];
    PPEB_LDR_DATA_ Ldr;
} PEB_, * PPEB_;

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#define NtCurrentProcess ((HANDLE)(LONG_PTR)-1)

typedef NTSTATUS(NTAPI* NtOpenProcess_t)(
    PHANDLE,
    ACCESS_MASK,
    POBJECT_ATTRIBUTES_,
    PCLIENT_ID_
    );

typedef NTSTATUS(NTAPI* NtReadVirtualMemory_t)(
    HANDLE,
    PVOID,
    PVOID,
    SIZE_T,
    PSIZE_T
    );

typedef NTSTATUS(NTAPI* NtClose_t)(
    HANDLE
    );

namespace mem
{
    constexpr uint32_t fnv1a(const char* s)
    {
        uint32_t h = 0x811c9dc5u;
        while (*s) { h ^= (uint8_t)*s++; h *= 0x01000193u; }
        return h;
    }

    struct BatchEntry
    {
        uintptr_t address = 0;
        void* buffer = nullptr;
        size_t size = 0;
        bool success = false;
    };

    struct ProcessModule
    {
        uintptr_t base = 0;
        size_t size = 0;
    };

    HMODULE get_ntdll();

    void* get_export(HMODULE mod, uint32_t hash);

    class process
    {
    public:
        process() = default;
        ~process() { detach(); }

        process(const process&) = delete;
        process& operator=(const process&) = delete;

        process(process&& o) noexcept
            : m_handle(o.m_handle) {
            o.m_handle = nullptr;
        }

        bool attach(DWORD pid);
        void detach();
        bool is_attached() const { return m_handle != nullptr; }

        template<typename T>
        T read(uintptr_t address) const
        {
            static_assert(std::is_trivially_copyable_v<T>,
                "mem::process::read<T> requires trivially copyable T");

            T value{};
            read_raw(address, &value, sizeof(T));
            return value;
        }

        size_t read_raw(uintptr_t address, void* buffer, size_t size) const;

        void read_batch(std::vector<BatchEntry>& entries) const;

        std::string read_string(uintptr_t address, size_t max_len = 256) const;

        uintptr_t find_pattern(ProcessModule mod, const char* pattern) const;

        uintptr_t resolve_rip(uintptr_t addr) const;

        uintptr_t get_module_base(const wchar_t* module_name) const;

        uintptr_t find_vtable(ProcessModule mod, const char* class_name) const;

        HANDLE native_handle() const { return m_handle; }

    private:
        HANDLE m_handle = nullptr;
        static bool resolve_functions();

        static NtReadVirtualMemory_t s_NtRead;
        static NtClose_t s_NtClose;
    };
}