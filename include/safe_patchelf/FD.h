#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <map>

class FD {
public:
    enum { BAD = -1 };

    explicit FD(int initial = BAD);

    FD(FD&& src);
    ~FD();

    void close();

    int get() const;
    bool bad() const;

    struct ::stat stat() const;

    size_t size() const;

    void* mmap(off_t off = 0, size_t in_len = 0, int prot = PROT_READ, int flags = MAP_FILE | MAP_SHARED);

private:
    // Do not copy
    FD(const FD&) = delete;

    int fd_;
    std::map<void*,size_t> maps_;
};
