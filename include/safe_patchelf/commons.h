#pragma once

#include <elf/elf.h>
#include <byteswap.h>

enum ElfClass {
    None    = 0,
    Elf32   = 32,
    Elf64   = 64,
};

enum Endian {
    Unknown = 0,
    Little  = 1,
    Big     = 2,
};

struct GetHostEndian {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    constexpr static const char* name       = "Little endian";
    constexpr static const Endian endian    = Little;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    constexpr static const char* name       = "Big endian";
    constexpr static const Endian endian    = Big;
#else
# error Unsupported host endian!
#endif
};

template<ElfClass Class> struct ElfClassTraits;

template<> struct ElfClassTraits<Elf32> {
    constexpr static const char* name = "ELF32";

    using Ehdr      = Elf32_Ehdr;
    using Phdr      = Elf32_Phdr;
    using Shdr      = Elf32_Shdr;
    using Dyn       = Elf32_Dyn;

    using Half      = Elf32_Half;
    using Word      = Elf32_Word;
    using Sword     = Elf32_Sword;
    using Xword     = Elf32_Xword;
    using Sxword    = Elf32_Sxword;

    using Addr      = Elf32_Addr;
    using Off       = Elf32_Off;
    using Section   = Elf32_Section;
    using Versym    = Elf32_Versym;
};

template<> struct ElfClassTraits<Elf64> {
    constexpr static const char* name = "ELF64";

    using Ehdr      = Elf64_Ehdr;
    using Phdr      = Elf64_Phdr;
    using Shdr      = Elf64_Shdr;
    using Dyn       = Elf64_Dyn;

    using Half      = Elf64_Half;
    using Word      = Elf64_Word;
    using Sword     = Elf64_Sword;
    using Xword     = Elf64_Xword;
    using Sxword    = Elf64_Sxword; 

    using Addr      = Elf64_Addr;
    using Off       = Elf64_Off;
    using Section   = Elf64_Section;
    using Versym    = Elf64_Versym;
};

struct Bswap {
    template<typename T, std::size_t SIZE>
    struct type_size_equals:
        std::integral_constant<bool, (sizeof(T)==SIZE)> {};

    template<
        typename T,
        typename std::enable_if<
            type_size_equals<T, 1>::value
            >::type* = nullptr
    >
    static T bswap(T inval) {
        return inval;
    }
    template<
        typename T,
        typename std::enable_if<
            type_size_equals<T, 2>::value
            >::type* = nullptr
    >
    static T bswap(T inval) {
        return __bswap_16(inval);
    }
    template<
        typename T,
        typename std::enable_if<
            type_size_equals<T, 4>::value
            >::type* = nullptr
    >
    static T bswap(T inval) {
        return __bswap_32(inval);
    }
    template<
        typename T,
        typename std::enable_if<
            type_size_equals<T, 8>::value
            >::type* = nullptr
    >
    static T bswap(T inval) {
        return __bswap_64(inval);
    }
};
