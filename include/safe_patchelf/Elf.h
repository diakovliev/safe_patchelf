#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <algorithm>

#include <cstring>

#include <safe_patchelf/commons.h>

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
