#ifndef TEST_FIRMWARE_STDOUT_CAPTURE_H
#define TEST_FIRMWARE_STDOUT_CAPTURE_H

// stdout capture helper shared by the firmware test binaries.
//
// Redirects fd 1 (stdout) to a pipe, lets the test drain whatever the
// desktop build of firmware::SerialProtocol wrote (see serial_protocol.cpp
// §write_json #else branch), restores stdout on scope exit.
// Linux/macOS only (POSIX pipe + dup2).

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <string>

class StdoutCapture {
public:
    StdoutCapture()
    {
        std::fflush(stdout);
        saved_fd_ = ::dup(1);
        if (::pipe(pipe_fds_) != 0) std::fprintf(stderr, "pipe() failed\n");
        ::dup2(pipe_fds_[1], 1);
        ::close(pipe_fds_[1]);
        // Make read end non-blocking so we can drain without hanging.
        int flags = ::fcntl(pipe_fds_[0], F_GETFL, 0);
        ::fcntl(pipe_fds_[0], F_SETFL, flags | O_NONBLOCK);
    }

    ~StdoutCapture()
    {
        std::fflush(stdout);
        ::dup2(saved_fd_, 1);
        ::close(saved_fd_);
        ::close(pipe_fds_[0]);
    }

    std::string drain()
    {
        std::fflush(stdout);
        std::string out;
        char buf[4096];
        ssize_t n;
        while ((n = ::read(pipe_fds_[0], buf, sizeof(buf))) > 0) {
            out.append(buf, static_cast<size_t>(n));
        }
        return out;
    }

private:
    int saved_fd_   = -1;
    int pipe_fds_[2] = { -1, -1 };
};

#endif // TEST_FIRMWARE_STDOUT_CAPTURE_H
