#include <iostream>
#include <fstream>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstring>
#include <fcntl.h>
#include <byteswap.h>

#include "FD.h"

#include "elf.h"

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
    using Dyn       = Elf32_Dyn;

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


template<ElfClass Class, Endian ElfEndian, Endian HostEndian = GetHostEndian::endian>
class Elf {
public:
    using Traits = ElfClassTraits<Class>;

    Elf(void *content)
        : content_(reinterpret_cast<caddr_t>(content))
        , ehdr_(reinterpret_cast<typename Traits::Ehdr*>(content))
    {
        std::cout << "   class: " << Traits::name << std::endl;
        std::cout << "endianes: " << (ElfEndian == Little ? "little" : "big") << std::endl;

        fill_headers();
    }

    typename Traits::Shdr* shstrtab() {
        return shdrs_[rdi(ehdr_->e_shstrndx)];
    }

    typename Traits::Shdr* find_section(const char *sh_name) {
        auto it = std::find_if(shdrs_.begin(), shdrs_.end(), [this, sh_name](auto* shdr){
            return strcmp(sh_name, section_name(shdr)) == 0;
        });
        if (it != shdrs_.end())
            return *it;
        else
            return nullptr;
    }

    const char *section_name(typename Traits::Shdr* shdr) {
        return content_ + rdi(shstrtab()->sh_offset) + rdi(shdr->sh_name);
    }


    caddr_t section_data(const char* section_name) {
        auto shdr = find_section(section_name);
        return content_ + rdi(shdr->sh_offset);
    }


    const char *soname(const char* new_soname = nullptr) {
        auto dynamic    = reinterpret_cast<typename Traits::Dyn*>(section_data(".dynamic"));
        if (!dynamic)
            return nullptr;

        auto dynstr     = section_data(".dynstr");
        if (!dynstr)
            return nullptr;

        char* soname = nullptr;
        for (auto dyn = dynamic; rdi(dyn->d_tag) != DT_NULL; ++dyn) {
            if (rdi(dyn->d_tag) == DT_SONAME) {
                soname = dynstr + rdi(dyn->d_un.d_val);
                break;
            }
        }

        if (new_soname) {
            std::cout << "new soname +: " << new_soname << std::endl;
            size_t old_soname_size = ::strlen(soname);
            ::strncpy(soname, new_soname, old_soname_size);
            std::cout << "soname -: " << soname << std::endl;
        }

        return soname;
    }

    std::vector<const char*> needed_list(const std::map<const char*, const char*>& replacements) {
        std::vector<const char*> result;

        auto dynamic    = reinterpret_cast<typename Traits::Dyn*>(section_data(".dynamic"));
        if (!dynamic)
            return result;

        auto dynstr     = section_data(".dynstr");
        if (!dynstr)
            return result;

        for (auto dyn = dynamic; rdi(dyn->d_tag) != DT_NULL; ++dyn) {
            if (rdi(dyn->d_tag) == DT_NEEDED) {
                char *needed_str = dynstr + rdi(dyn->d_un.d_val);

                std::for_each(replacements.begin(), replacements.end(), [needed_str](auto& it) {
                    if (::strcmp(it.first, needed_str) != 0)
                        return;

                    std::cout << "new needed +: " << it.second << std::endl;
                    size_t old_needed_size = ::strlen(needed_str);
                    ::strncpy(needed_str, it.second, old_needed_size);
                    std::cout << "needed -: " << needed_str << std::endl;
                });

                result.push_back(needed_str);
            }
        }
        return result;
    }

protected:

    template<typename T>
    T rdi(T elf_val) {
        if (HostEndian == ElfEndian)
            return elf_val;

        return Bswap::bswap<T>(elf_val);
    }

    template<typename T>
    T wdi(T host_val) {
        return rdi<T>(host_val);
    }

    void fill_headers() {
        for (int i = 0; i < rdi(ehdr_->e_phnum); ++i)
            phdrs_.push_back(&((typename Traits::Phdr*)(content_ + rdi(ehdr_->e_phoff)))[i]);

        std::cout << "segments headers: " << phdrs_.size() << std::endl;

        for (int i = 0; i < rdi(ehdr_->e_shnum); ++i)
            shdrs_.push_back(&((typename Traits::Shdr*)(content_ + rdi(ehdr_->e_shoff)))[i]);

        std::cout << "sections headers: " << shdrs_.size() << std::endl;
        std::for_each(shdrs_.begin(), shdrs_.end(), [this](auto* shdr) {
            std::cout << "\t"  << shdr << ": " << section_name(shdr) << std::endl;
        });

        auto strtab = find_section(".strtab");
        if (strtab)
            std::cout << " .strtab: " << strtab << std::endl;


        //auto needed = needed_list();
        //std::cout << "needed:" << std::endl;
        //std::for_each(needed.begin(), needed.end(), [](const auto *n) {
        //    std::cout << "\t" << n << std::endl;
        //});

        //std::cout << "soname: " << soname() << std::endl;
    }

private:
    caddr_t content_;
    typename Traits::Ehdr* ehdr_;
    std::vector<typename Traits::Phdr*> phdrs_;
    std::vector<typename Traits::Shdr*> shdrs_;
};

template<ElfClass Class>
int class_entry(void* content, Endian elf_endian) {
    std::map<const char*, const char*> replacements;
    replacements.insert(std::make_pair("libdiskmgr.so", "libdxxxxxx.so"));
                                                      //"libdiskmgr.so"
    if (elf_endian == Little) {
        Elf<Class, Little> elf(content);
                   //"libstorage.so"
        elf.soname("libnsoname.so");
        elf.needed_list(replacements);
        return 0;
    } else if (elf_endian == Big) {
        Elf<Class, Big> elf(content);
        elf.soname("libnsoname.so");
        elf.needed_list(replacements);
        return 0;
    }
    return -1;
}

std::pair<ElfClass, Endian> elf_class(caddr_t contents) {
    if (::memcmp(contents, ELFMAG, SELFMAG) != 0)
        return std::make_pair(None, Unknown);

    if (contents[EI_VERSION] != EV_CURRENT)
        return std::make_pair(None, Unknown);

    Endian elf_endian = contents[EI_DATA] == ELFDATA2LSB ? Little : Big;

    if (contents[EI_CLASS] == ELFCLASS32)
        return std::make_pair(Elf32, elf_endian);

    if (contents[EI_CLASS] == ELFCLASS64)
        return std::make_pair(Elf64, elf_endian);

    return std::make_pair(None, Unknown);
}

int main(int argc, char** argv) {

    if (argc < 2) {
        std::cerr << "No input file specified!" << std::endl;
        return -1;
    }

    const char *source_file = argv[1];

    FD fd(::open(source_file, O_RDWR));
    if (fd.bad()) {
        std::cerr << "Can't open " << source_file << "!" << std::endl;
        return -1;
    }

    std::cout << "    host: " << GetHostEndian::name << std::endl;
    std::cout << "filename: " << source_file << std::endl;
    std::cout << "    size: " << fd.size()   << std::endl;

    caddr_t content = reinterpret_cast<caddr_t>(fd.mmap(0, 0, PROT_READ|PROT_WRITE));
    std::cout << " content: " << (void*)content << std::endl;

    auto el_class = elf_class(content);
    switch(el_class.first) {
    case Elf32:
    {
        return class_entry<Elf32>(content, el_class.second);
    }
    break;
    case Elf64:
    {
        return class_entry<Elf64>(content, el_class.second);
    }
    break;
    default:
        std::cerr << source_file << ": not an ELF file!";
        return -1;
    };
}
