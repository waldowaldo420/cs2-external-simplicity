#include "memory.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace mem
{
    NtReadVirtualMemory_t process::s_NtRead = nullptr;
    NtClose_t process::s_NtClose = nullptr;

    HMODULE get_ntdll()
    {
#ifdef _M_X64
        auto* peb = reinterpret_cast<PEB_*>(__readgsqword(0x60));
#else
        auto* peb = reinterpret_cast<PEB_*>(__readfsdword(0x30));
#endif
        if (!peb || !peb->Ldr) return nullptr;

        auto* head = &peb->Ldr->InMemoryOrderModuleList;
        for (auto* e = head->Flink; e != head; e = e->Flink)
        {
            auto* entry = reinterpret_cast<PLDR_DATA_TABLE_ENTRY_>(
                reinterpret_cast<BYTE*>(e) - sizeof(LIST_ENTRY));

            if (!entry->BaseDllName.Buffer) continue;

            const wchar_t* name = entry->BaseDllName.Buffer;

            if ((name[0] | 0x20) == L'n' &&
                (name[1] | 0x20) == L't' &&
                (name[2] | 0x20) == L'd')
            {
                return reinterpret_cast<HMODULE>(entry->DllBase);
            }
        }
        return nullptr;
    }

    void* get_export(HMODULE mod, uint32_t hash)
    {
        if (!mod) return nullptr;

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<BYTE*>(mod) + dos->e_lfanew);

        const auto& dir = nt->OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return nullptr;

        auto* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
            reinterpret_cast<BYTE*>(mod) + dir.VirtualAddress);

        auto* names = reinterpret_cast<DWORD*>(
            reinterpret_cast<BYTE*>(mod) + exports->AddressOfNames);
        auto* funcs = reinterpret_cast<DWORD*>(
            reinterpret_cast<BYTE*>(mod) + exports->AddressOfFunctions);
        auto* ords = reinterpret_cast<WORD*>(
            reinterpret_cast<BYTE*>(mod) + exports->AddressOfNameOrdinals);

        for (DWORD i = 0; i < exports->NumberOfNames; i++)
        {
            const char* name = reinterpret_cast<const char*>(
                reinterpret_cast<BYTE*>(mod) + names[i]);

            if (fnv1a(name) == hash)
                return reinterpret_cast<BYTE*>(mod) + funcs[ords[i]];
        }
        return nullptr;
    }

    bool mem::process::resolve_functions()
    {
        HMODULE ntdll = get_ntdll();
        if (!ntdll)
        {
            std::cout << "[mem] failed to find ntdll\n";
            return false;
        }
        s_NtRead = reinterpret_cast<NtReadVirtualMemory_t>(
            get_export(ntdll, fnv1a("NtReadVirtualMemory")));
        s_NtClose = reinterpret_cast<NtClose_t>(
            get_export(ntdll, fnv1a("NtClose")));
        if (!s_NtRead || !s_NtClose)
        {
            std::cout << "[mem] failed to resolve NT functions\n";
            return false;
        }
        std::cout << "[mem] NT functions resolved\n";
        return true;
    }

    bool process::attach(DWORD pid)
    {
        detach();

        static bool resolved = resolve_functions();
        if (!resolved) return false;

        HMODULE ntdll = get_ntdll();
        auto NtOpenProc = reinterpret_cast<NtOpenProcess_t>(
            get_export(ntdll, fnv1a("NtOpenProcess")));

        if (!NtOpenProc)
        {
            std::cout << "[mem] NtOpenProcess not found\n";
            return false;
        }

        CLIENT_ID_ cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(
            static_cast<ULONG_PTR>(pid));
        cid.UniqueThread = nullptr;

        OBJECT_ATTRIBUTES_ oa{};
        oa.Length = sizeof(oa);

        NTSTATUS status = NtOpenProc(
            &m_handle,
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            &oa,
            &cid);

        if (!NT_SUCCESS(status))
        {
            std::cout << "[mem] NtOpenProcess failed: 0x"
                << std::hex << status << "\n";
            m_handle = nullptr;
            return false;
        }

        std::cout << "[mem] attached to PID " << std::dec << pid << "\n";
        return true;
    }

    void process::detach()
    {
        if (!m_handle) return;

        if (s_NtClose)
            s_NtClose(m_handle);
        else
            CloseHandle(m_handle);

        m_handle = nullptr;
    }

    size_t process::read_raw(uintptr_t address,
        void* buffer,
        size_t    size) const
    {
        if (!m_handle || !buffer || !address || !size) return 0;

        SIZE_T bytes_read = 0;
        NTSTATUS status = s_NtRead(
            m_handle,
            reinterpret_cast<PVOID>(address),
            buffer,
            size,
            &bytes_read);

        return NT_SUCCESS(status) ? bytes_read : 0;
    }

    void process::read_batch(std::vector<BatchEntry>& entries) const
    {
        if (!m_handle) return;

        for (auto& entry : entries)
        {
            if (!entry.address || !entry.buffer || !entry.size)
            {
                entry.success = false;
                continue;
            }

            SIZE_T bytes_read = 0;
            NTSTATUS status = s_NtRead(
                m_handle,
                reinterpret_cast<PVOID>(entry.address),
                entry.buffer,
                entry.size,
                &bytes_read);

            entry.success = NT_SUCCESS(status) && bytes_read == entry.size;
        }
    }

    std::string process::read_string(uintptr_t address,
        size_t    max_len) const
    {
        if (!address) return {};

        std::vector<char> buf(max_len, '\0');
        size_t read = read_raw(address, buf.data(), max_len - 1);
        if (!read) return {};

        buf[read] = '\0';
        return std::string(buf.data());
    }

    uintptr_t process::resolve_rip(uintptr_t addr) const
    {
        if (!m_handle) return 0;
        const auto disp = read<int32_t>(addr);
        return addr + 4 + static_cast<uintptr_t>(static_cast<int64_t>(disp));
    }

    uintptr_t process::find_pattern(ProcessModule mod, const char* pattern) const
    {
        if (!m_handle || !mod.base || !mod.size) return 0;

        struct byte_entry { uint8_t val; bool wild; };
        std::vector<byte_entry> pat;
        pat.reserve(32);

        auto hex = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            return 0;
            };

        for (const char* p = pattern; *p;)
        {
            while (*p == ' ') ++p;
            if (!*p) break;
            if (p[0] == '?') {
                pat.push_back({ 0, true });
                ++p;
                if (*p == '?') ++p;
            }
            else {
                uint8_t b = (hex(p[0]) << 4) | hex(p[1]);
                pat.push_back({ b, false });
                p += 2;
            }
        }
        if (pat.empty()) return 0;

        constexpr size_t k_chunk = 0x10000;
        const size_t     overlap = pat.size() - 1;
        std::vector<uint8_t> buf(k_chunk + overlap);

        for (size_t offset = 0; offset < mod.size; offset += k_chunk)
        {
            const size_t read_size = (std::min)(k_chunk + overlap, mod.size - offset);
            if (!read_raw(mod.base + offset, buf.data(), read_size))
                continue;

            for (size_t i = 0; i + pat.size() <= read_size; ++i)
            {
                bool ok = true;
                for (size_t j = 0; j < pat.size(); ++j)
                {
                    if (!pat[j].wild && buf[i + j] != pat[j].val)
                    {
                        ok = false;
                        break;
                    }
                }
                if (ok) return mod.base + offset + i;
            }
        }
        return 0;
    }

    uintptr_t process::get_module_base(const wchar_t* module_name) const
    {
        if (!m_handle || !module_name) return 0;

        typedef struct {
            NTSTATUS ExitStatus;
            PVOID PebBaseAddress;
            ULONG_PTR AffinityMask;
            LONG  BasePriority;
            ULONG_PTR UniqueProcessId;
            ULONG_PTR InheritedFromUniqueProcessId;
        } PROCESS_BASIC_INFORMATION_;

        typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(
            HANDLE, ULONG, PVOID, ULONG, PULONG);

        HMODULE ntdll = get_ntdll();
        auto NtQIP = reinterpret_cast<NtQueryInformationProcess_t>(
            get_export(ntdll, fnv1a("NtQueryInformationProcess")));

        if (!NtQIP) return 0;

        PROCESS_BASIC_INFORMATION_ pbi{};
        ULONG ret_len = 0;
        if (!NT_SUCCESS(NtQIP(m_handle, 0, &pbi, sizeof(pbi), &ret_len)))
            return 0;

        if (!pbi.PebBaseAddress) return 0;

        PEB_ peb{};
        if (!read_raw(reinterpret_cast<uintptr_t>(pbi.PebBaseAddress),
            &peb, sizeof(peb))) return 0;

        if (!peb.Ldr) return 0;

        PEB_LDR_DATA_ ldr{};
        if (!read_raw(reinterpret_cast<uintptr_t>(peb.Ldr),
            &ldr, sizeof(ldr))) return 0;

        const uintptr_t head = reinterpret_cast<uintptr_t>(peb.Ldr)
            + offsetof(PEB_LDR_DATA_, InMemoryOrderModuleList);

        uintptr_t current = reinterpret_cast<uintptr_t>(
            ldr.InMemoryOrderModuleList.Flink);

        while (current && current != head)
        {
            LDR_DATA_TABLE_ENTRY_ entry{};
            if (!read_raw(current - sizeof(LIST_ENTRY),
                &entry, sizeof(entry))) break;

            if (entry.BaseDllName.Buffer && entry.BaseDllName.Length > 0)
            {
                const size_t name_len = entry.BaseDllName.Length / sizeof(wchar_t);
                std::vector<wchar_t> name_buf(name_len + 1, L'\0');

                if (read_raw(reinterpret_cast<uintptr_t>(
                    entry.BaseDllName.Buffer),
                    name_buf.data(),
                    entry.BaseDllName.Length))
                {
                    bool match = true;
                    const wchar_t* target = module_name;
                    const wchar_t* src = name_buf.data();

                    while (*target && *src)
                    {
                        if (((*target) | 0x20) != ((*src) | 0x20))
                        {
                            match = false;
                            break;
                        }
                        target++; src++;
                    }
                    if (match && !*target && !*src)
                        return reinterpret_cast<uintptr_t>(entry.DllBase);
                }
            }

            LIST_ENTRY link{};
            if (!read_raw(current, &link, sizeof(link))) break;
            current = reinterpret_cast<uintptr_t>(link.Flink);
        }

        return 0;
    }

    uintptr_t process::find_vtable(ProcessModule mod, const char* class_name) const
    {
        if (!m_handle || !mod.base || !mod.size) return 0;

        const std::string descriptor_name = std::string(".?AV") + class_name + "@@";
        constexpr size_t k_chunk = 0x10000;

        uintptr_t type_descriptor = 0;
        {
            const auto dos = read<IMAGE_DOS_HEADER>(mod.base);
            if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
            const auto nt = read<IMAGE_NT_HEADERS>(mod.base + dos.e_lfanew);
            if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;

            const uintptr_t first_sec = mod.base + dos.e_lfanew
                + offsetof(IMAGE_NT_HEADERS, OptionalHeader)
                + nt.FileHeader.SizeOfOptionalHeader;

            for (uint16_t si = 0; si < nt.FileHeader.NumberOfSections && !type_descriptor; ++si)
            {
                const auto hdr = read<IMAGE_SECTION_HEADER>(
                    first_sec + si * sizeof(IMAGE_SECTION_HEADER));

                constexpr DWORD need = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
                if ((hdr.Characteristics & need) != need) continue;
                if (hdr.Misc.VirtualSize <= descriptor_name.size()) continue;

                const uintptr_t sec_base = mod.base + hdr.VirtualAddress;
                const size_t    sec_size = hdr.Misc.VirtualSize;
                const size_t    overlap = descriptor_name.size();

                std::vector<uint8_t> buf(k_chunk + overlap);

                for (size_t off = 0; off < sec_size && !type_descriptor; off += k_chunk)
                {
                    const size_t rs = std::min(k_chunk + overlap, sec_size - off);
                    if (!read_raw(sec_base + off, buf.data(), rs)) continue;

                    for (size_t i = 0; i + descriptor_name.size() < rs; ++i)
                    {
                        if (std::memcmp(buf.data() + i,
                            descriptor_name.data(),
                            descriptor_name.size() + 1) == 0)
                        {
                            type_descriptor = sec_base + off + i - 0x10;
                            break;
                        }
                    }
                }
            }
        }
        if (!type_descriptor) return 0;

        const uint32_t descriptor_rva =
            static_cast<uint32_t>(type_descriptor - mod.base);

        uintptr_t col_address = 0;
        {
            const auto dos = read<IMAGE_DOS_HEADER>(mod.base);
            const auto nt = read<IMAGE_NT_HEADERS>(mod.base + dos.e_lfanew);
            const uintptr_t first_sec = mod.base + dos.e_lfanew
                + offsetof(IMAGE_NT_HEADERS, OptionalHeader)
                + nt.FileHeader.SizeOfOptionalHeader;

            for (uint16_t si = 0; si < nt.FileHeader.NumberOfSections && !col_address; ++si)
            {
                const auto hdr = read<IMAGE_SECTION_HEADER>(
                    first_sec + si * sizeof(IMAGE_SECTION_HEADER));

                char name[9]{}; std::memcpy(name, hdr.Name, 8);
                if (std::string_view{ name }.find(".rdata") == std::string_view::npos) continue;
                if (hdr.Misc.VirtualSize < 0x30) continue;

                const uintptr_t sec_base = mod.base + hdr.VirtualAddress;
                const size_t    sec_size = hdr.Misc.VirtualSize;

                std::vector<uint8_t> buf(k_chunk + 0x30);

                for (size_t off = 0; off < sec_size && !col_address; off += k_chunk)
                {
                    const size_t rs = std::min(k_chunk + size_t(0x30), sec_size - off);
                    if (!read_raw(sec_base + off, buf.data(), rs)) continue;

                    for (size_t i = 0; i + 0x30 <= rs; i += 8)
                    {
                        const auto* dw = reinterpret_cast<const uint32_t*>(buf.data() + i);
                        if (dw[3] == descriptor_rva)
                        {
                            col_address = sec_base + off + i;
                            break;
                        }
                    }
                }
            }
        }
        if (!col_address) return 0;

        {
            const auto dos = read<IMAGE_DOS_HEADER>(mod.base);
            const auto nt = read<IMAGE_NT_HEADERS>(mod.base + dos.e_lfanew);
            const uintptr_t first_sec = mod.base + dos.e_lfanew
                + offsetof(IMAGE_NT_HEADERS, OptionalHeader)
                + nt.FileHeader.SizeOfOptionalHeader;

            for (uint16_t si = 0; si < nt.FileHeader.NumberOfSections; ++si)
            {
                const auto hdr = read<IMAGE_SECTION_HEADER>(
                    first_sec + si * sizeof(IMAGE_SECTION_HEADER));

                if (!(hdr.Characteristics & IMAGE_SCN_MEM_READ)) continue;
                if (hdr.Misc.VirtualSize < 8) continue;

                const uintptr_t sec_base = mod.base + hdr.VirtualAddress;
                const size_t    sec_size = hdr.Misc.VirtualSize;

                std::vector<uint8_t> buf(k_chunk + 8);

                for (size_t off = 0; off < sec_size; off += k_chunk)
                {
                    const size_t rs = std::min(k_chunk + size_t(8), sec_size - off);
                    if (!read_raw(sec_base + off, buf.data(), rs)) continue;

                    for (size_t i = 0; i + 8 <= rs; i += 8)
                    {
                        const auto ptr = *reinterpret_cast<const uintptr_t*>(buf.data() + i);
                        if (ptr == col_address)
                            return sec_base + off + i + 8;
                    }
                }
            }
        }
        return 0;
    }
}