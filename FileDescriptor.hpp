#pragma once

#include <unistd.h>
#include <utility>

// RAII owner of a single POSIX file descriptor.
//
// The rule this class enforces: a fd has exactly one owner at a time.
// - Constructing it "acquires" an fd you already opened (open/pipe/dup...).
// - Destroying it closes the fd automatically, on every exit path
//   (normal return, early return, exception).
// - Copying is disabled (two owners closing the same fd is a bug).
// - Moving is allowed (ownership transfers cleanly, e.g. into a Job).
//
// This does NOT open files itself — it just owns fds you already got
// from a syscall. Keep syscalls explicit and visible at the call site;
// that's the part you actually want to understand, not hide.
class FileDescriptor {
public:
    FileDescriptor() = default;

    explicit FileDescriptor(int fd) : fd_(fd) {}

    ~FileDescriptor() {
        reset();
    }

    // No copies: two objects must never both believe they own the same fd.
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    // Moves are fine: ownership transfers, source becomes empty.
    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    // Raw fd for passing into syscalls (dup2, read, write, ...).
    int get() const { return fd_; }

    bool valid() const { return fd_ >= 0; }

    // Close the owned fd now (if any), leaving this object empty.
    // Useful when you need to close a pipe end at a *specific* point
    // in the code rather than waiting for scope exit — e.g. the parent
    // closing the read end of a pipe right after forking.
    void reset() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    // Give up ownership WITHOUT closing. Use this when something else
    // is about to take over the fd's lifetime (rare — most of the time
    // you want reset(), not release()).
    int release() {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

private:
    int fd_ = -1;
};
