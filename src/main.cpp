#include <iostream>
#include <fstream>
#include <vector>
#include <optional>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstring>
#include <fcntl.h>
#include <byteswap.h>
#include <getopt.h>

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


template<ElfClass Class, Endian ElfEndian, Endian HostEndian = GetHostEndian::endian>
class Elf {
public:
    using Traits = ElfClassTraits<Class>;

    Elf(void *content)
        : content_(reinterpret_cast<caddr_t>(content))
        , ehdr_(reinterpret_cast<typename Traits::Ehdr*>(content))
        , phdrs_()
        , shdrs_()
        , executable_(false)
    {
        std::cout << "     class: " << Traits::name << std::endl;
        std::cout << "  endianes: " << (ElfEndian == Little ? "little" : "big") << std::endl;

        fill_headers();

        std::cout << "executable: " << executable_ << std::endl;
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


    bool set_soname(const char* new_soname) {
        bool result = false;

        // Can't set soname for executable
        if (executable_) {
            std::cerr << "error: Can't set soname for executable!" << std::endl;
            return result;
        }

        auto dynamic    = reinterpret_cast<typename Traits::Dyn*>(section_data(".dynamic"));
        if (!dynamic) {
            std::cerr << "error: Can't find .dynamic section!" << std::endl;
            return result;
        }

        auto dynstr     = section_data(".dynstr");
        if (!dynstr) {
            std::cerr << "error: Can't find .dynstr section!" << std::endl;
            return result;
        }

        char* soname = nullptr;
        for (auto dyn = dynamic; rdi(dyn->d_tag) != DT_NULL; ++dyn) {
            if (rdi(dyn->d_tag) == DT_SONAME) {
                soname = dynstr + rdi(dyn->d_un.d_val);
                break;
            }
        }

        if (!soname) {
            std::cerr << "error: Can't find soname record in .dynamic!" << std::endl;
            return result;
        }

        if (new_soname && ::strcmp(new_soname, soname) != 0) {
            std::cout << "new soname +: " << new_soname << std::endl;
            size_t old_soname_size = ::strlen(soname);
            ::strncpy(soname, new_soname, old_soname_size);
            std::cout << "soname -: " << soname << std::endl;
            result = true;
        }

        if (!result) {
            std::cerr << "error: New soname was not set!" << std::endl;
        }

        return result;
    }

    bool update_neededs(const std::map<std::string, std::string>& replacements) {
        bool result = false;

        auto dynamic    = reinterpret_cast<typename Traits::Dyn*>(section_data(".dynamic"));
        if (!dynamic) {
            std::cerr << "error: Can't find .dynamic section!" << std::endl;
            return result;
        }

        auto dynstr     = section_data(".dynstr");
        if (!dynstr) {
            std::cerr << "error: Can't find .dynstr section!" << std::endl;
            return result;
        }

        for (auto dyn = dynamic; rdi(dyn->d_tag) != DT_NULL; ++dyn) {
            if (rdi(dyn->d_tag) == DT_NEEDED) {
                char *needed_str = dynstr + rdi(dyn->d_un.d_val);

                std::for_each(replacements.begin(), replacements.end(), [&](auto& it) {
                    if (::strcmp(it.first.c_str(), needed_str) != 0)
                        return;

                    std::cout << "new needed +: " << it.second << std::endl;
                    size_t old_needed_size = ::strlen(needed_str);
                    ::strncpy(needed_str, it.second.c_str(), old_needed_size);
                    std::cout << "needed -: " << needed_str << std::endl;

                    result = true;
                });
            }
        }

        if (!result) {
            std::cerr << "error: No requested updates in neededs!" << std::endl;
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
        for (int i = 0; i < rdi(ehdr_->e_phnum); ++i) {
            phdrs_.push_back(&((typename Traits::Phdr*)(content_ + rdi(ehdr_->e_phoff)))[i]);
            if (rdi(phdrs_[i]->p_type) == PT_INTERP) executable_ = true;
        }

        for (int i = 0; i < rdi(ehdr_->e_shnum); ++i)
            shdrs_.push_back(&((typename Traits::Shdr*)(content_ + rdi(ehdr_->e_shoff)))[i]);

        //std::cout << "Sections:" << std::endl;
        //std::for_each(shdrs_.begin(), shdrs_.end(), [&](auto* shdr) {
        //    std::cout << "\t" << section_name(shdr) << std::endl;
        //});
    }

private:
    caddr_t content_;
    typename Traits::Ehdr* ehdr_;
    std::vector<typename Traits::Phdr*> phdrs_;
    std::vector<typename Traits::Shdr*> shdrs_;
    bool executable_;
};

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

struct Args {
    std::string filename;
    std::string soname;
    std::map<std::string, std::string> neededs;

    static std::optional<std::pair<std::string, std::string> > parse_needed(const char* n) {
        std::string s(n);
        auto it = s.find(',');
        if (it == std::string::npos)
           return std::nullopt;

        std::string old_needed(s.c_str(), it);
        std::string new_needed(s.c_str() + it + 1, s.size() - it);

        if (old_needed.empty() || new_needed.empty())
           return std::nullopt;

        return std::make_pair(old_needed, new_needed);
    }

    void print(std::ostream& out = std::cout) const {
        out << "Arguments:" << std::endl;
        if (!filename.empty())
            out << "\tinput file: " << filename << std::endl;
        if (!soname.empty())
            out << "\tnew soname: " << soname << std::endl;
        std::for_each(neededs.begin(), neededs.end(), [&](auto& n) {
            out << "\tnew needed: " << n.first << " -> " << n.second << std::endl;
        });
    }

    static void show_usage(const char *program_name, std::ostream& out = std::cerr) {
        out << "Usage: " << program_name << " <options>"                                  << std::endl;
        out << "Were options are:"                                                        << std::endl;
        out << "\t-f,--filename: File to patch."                                          << std::endl;
        out << "\t-s,--soname  : New ELF soname."                                         << std::endl;
        out << "\t-n,--needed  : New ELF needed in format: <old needed>,<new needed>."    << std::endl;
        out << "\t-h,-?        : Show this help message."                                 << std::endl;
    }

    static std::optional<Args> parse_args(int argc, char** argv) {
        Args args;

        static const char *opt_string = "f:s:n:h?";

        static const struct option long_opts[] = {
            { "filename",   required_argument,  NULL, 'f' },
            { "soname",     required_argument,  NULL, 's' },
            { "needed",     required_argument,  NULL, 'n' },
            { NULL,         no_argument,        NULL, 0 }
        };

        int long_index = 0;

        int opt = getopt_long(argc, argv, opt_string, long_opts, &long_index);
        while( opt != -1 ) {
            switch(opt) {
            case 'f':
                args.filename = optarg;
            break;
            case 's':
                args.soname = optarg;
            break;
            case 'n':
            {
                auto n = parse_needed(optarg);
                if (!n) {
                    std::cerr << "error: Wrong needed replacement option: " << optarg << std::endl;
                    return std::nullopt;
                }
                args.neededs.insert(*n);
            }
            break;
            case 0:
                {
                    switch(long_index) {
                    case 0:
                        args.filename = optarg;
                    break;
                    case 1:
                        args.soname = optarg;
                    break;
                    case 2:
                    case 'n':
                    {
                        auto n = parse_needed(optarg);
                        if (!n) {
                            std::cerr << "error: Wrong needed replacement option: " << optarg << std::endl;
                            return std::nullopt;
                        }
                        args.neededs.insert(*n);
                    }
                    break;
                    default:
                        show_usage(argv[0]);
                        return std::nullopt;
                    }
                }
                break;
            case 'h':
            case '?':
            default:
                show_usage(argv[0]);
                return std::nullopt;
            };

            opt = getopt_long(argc, argv, opt_string, long_opts, &long_index);
        }

        if (args.filename.empty()) {
            std::cerr << "error: No ELF to process!" << std::endl;
            show_usage(argv[0]);
            return std::nullopt;
        }

        return args;
    }

    bool have_work() const {
        if (soname.empty() && neededs.empty())
            return false;

        return true;
    }

};

template<ElfClass Class>
int class_entry(void* content, Endian elf_endian, const Args& args) {
    bool sucess = true;

    if (elf_endian == Little) {
        Elf<Class, Little> elf(content);
        if (!args.soname.empty())
            sucess &= elf.set_soname(args.soname.c_str());
        if (!args.neededs.empty())
            sucess &= elf.update_neededs(args.neededs);
    } else if (elf_endian == Big) {
        Elf<Class, Big> elf(content);
        if (!args.soname.empty())
            sucess &= elf.set_soname(args.soname.c_str());
        if (!args.neededs.empty())
            sucess &= elf.update_neededs(args.neededs);
    }

    if (!sucess)
        return -1;
    else
        return 0;
}

int main(int argc, char** argv) {

    auto args = Args::parse_args(argc, argv);
    if (!args) {
        return -1;
    }

    if (!args->have_work()) {
        std::cerr << "error: Nothing to do!" << std::endl;
        return -1;
    }

    args->print();

    FD fd(::open(args->filename.c_str(), O_RDWR));
    if (fd.bad()) {
        std::cerr << "Can't open " << args->filename << "!" << std::endl;
        return -1;
    }

    std::cout << "    host: " << GetHostEndian::name << std::endl;
    std::cout << "    size: " << fd.size()   << std::endl;

    caddr_t content = reinterpret_cast<caddr_t>(fd.mmap(0, 0, PROT_READ|PROT_WRITE));
    std::cout << " content: " << (void*)content << std::endl;

    auto el_class = elf_class(content);
    switch(el_class.first) {
    case Elf32:
    {
        return class_entry<Elf32>(content, el_class.second, *args);
    }
    break;
    case Elf64:
    {
        return class_entry<Elf64>(content, el_class.second, *args);
    }
    break;
    default:
        std::cerr << args->filename << ": not an ELF file!";
        return -1;
    };
}
