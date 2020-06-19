#include <iostream>

#include <fcntl.h>

#include <safe_patchelf/commons.h>
#include <safe_patchelf/FD.h>
#include <safe_patchelf/Elf.h>
#include <safe_patchelf/Args.h>


struct DoElfPatching {
    template<class E>
    static bool entry(E& elf, const Args& args) {
        bool success = true;

        if (!args.soname.empty())
            success &= elf.set_soname(args.soname.c_str());

        if (!args.neededs.empty())
            success &= elf.update_neededs(args.neededs);

        if (!elf.results().empty()) {
            std::for_each(elf.results().begin(), elf.results().end(), [](auto& it) {
                if (it.first)
                    std::cerr << it.second << std::endl;
                else
                    std::cout << it.second << std::endl;
            });
        }

        return success;
    }
};


template<ElfClass Class, class Worker>
int class_entry(void* content, Endian elf_endian, const Args& args) {
    bool success = false;

    if (elf_endian == Little) {
        using LElf = Elf<Class, Little>;
        LElf elf(content);
        success = Worker::entry(elf, args);
    } else if (elf_endian == Big) {
        using BElf = Elf<Class, Big>;
        BElf elf(content);
        success = Worker::entry(elf, args);
    }

    if (!success)
        return -1;
    else
        return 0;
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

    auto args = Args::parse_args(argc, argv);
    if (!args) {
        return -1;
    }

    args->print();

    if (!args->have_work()) {
        std::cerr << "error: Nothing to do!" << std::endl;
        return -1;
    }

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
        return class_entry<Elf32, DoElfPatching>(content, el_class.second, *args);
    }
    break;
    case Elf64:
    {
        return class_entry<Elf64, DoElfPatching>(content, el_class.second, *args);
    }
    break;
    default:
        std::cerr << args->filename << ": not an ELF file!";
        return -1;
    };
}
