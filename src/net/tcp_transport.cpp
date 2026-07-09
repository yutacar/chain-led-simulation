/*
 * SPDX-FileCopyrightText: 2026 yutacar
 *
 * SPDX-License-Identifier: MIT
 */
#include "tcp_transport.h"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace chainled::net {
namespace {

bool set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void enable_nodelay(int fd) {
    const int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

std::string format_peer_address(const sockaddr_in& addr) {
    char ip_buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf));
    return std::string(ip_buf) + ":" + std::to_string(ntohs(addr.sin_port));
}

}  // namespace

TcpTransport::~TcpTransport() { close(); }

bool TcpTransport::listen(uint16_t port) {
    close();

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        return false;
    }

    const int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        fail();
        return false;
    }

    socklen_t addr_len = sizeof(addr);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
        local_port_ = ntohs(addr.sin_port);
    }

    if (::listen(listen_fd_, 1) != 0 || !set_nonblocking(listen_fd_)) {
        fail();
        return false;
    }

    state_ = TransportState::Connecting;
    return true;
}

bool TcpTransport::connect(const std::string& host, uint16_t port) {
    close();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0 || !result) {
        return false;
    }

    fd_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd_ < 0 || !set_nonblocking(fd_)) {
        freeaddrinfo(result);
        fail();
        return false;
    }

    const int rc = ::connect(fd_, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    if (rc == 0) {
        enable_nodelay(fd_);
        peer_address_ = host + ":" + std::to_string(port);
        state_ = TransportState::Connected;
    } else if (errno == EINPROGRESS) {
        peer_address_ = host + ":" + std::to_string(port);
        state_ = TransportState::Connecting;
    } else {
        fail();
        return false;
    }
    return true;
}

void TcpTransport::service_accept() {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    const int accepted = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (accepted < 0) {
        return;  // EAGAIN/EWOULDBLOCK: no pending connection yet
    }

    set_nonblocking(accepted);
    enable_nodelay(accepted);
    fd_ = accepted;
    peer_address_ = format_peer_address(peer);
    state_ = TransportState::Connected;

    ::close(listen_fd_);
    listen_fd_ = -1;
}

void TcpTransport::service_connect() {
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(fd_, &write_set);
    timeval timeout{0, 0};

    if (::select(fd_ + 1, nullptr, &write_set, nullptr, &timeout) <= 0) {
        return;  // not writable yet
    }

    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) != 0 || socket_error != 0) {
        fail();
        return;
    }

    enable_nodelay(fd_);
    state_ = TransportState::Connected;
}

void TcpTransport::service_io() {
    if (!send_buffer_.empty()) {
        const ssize_t written = ::send(fd_, send_buffer_.data(), send_buffer_.size(), 0);
        if (written > 0) {
            send_buffer_.erase(send_buffer_.begin(), send_buffer_.begin() + written);
        } else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            fail();
            return;
        }
    }

    uint8_t read_buf[2048];
    while (true) {
        const ssize_t got = ::recv(fd_, read_buf, sizeof(read_buf), 0);
        if (got > 0) {
            recv_buffer_.insert(recv_buffer_.end(), read_buf, read_buf + got);
            continue;
        }
        if (got == 0) {
            fail();  // orderly peer shutdown
            return;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            fail();
            return;
        }
        break;
    }
}

void TcpTransport::poll() {
    if (state_ == TransportState::Connecting) {
        if (listen_fd_ >= 0) {
            service_accept();
        } else if (fd_ >= 0) {
            service_connect();
        }
    }
    if (state_ == TransportState::Connected) {
        service_io();
    }
}

bool TcpTransport::send(const uint8_t* data, size_t len) {
    if (state_ != TransportState::Connected) {
        return false;
    }
    send_buffer_.insert(send_buffer_.end(), data, data + len);
    service_io();
    return true;
}

size_t TcpTransport::receive(std::vector<uint8_t>& out) {
    const size_t count = recv_buffer_.size();
    out.insert(out.end(), recv_buffer_.begin(), recv_buffer_.end());
    recv_buffer_.clear();
    return count;
}

void TcpTransport::fail() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    state_ = TransportState::Disconnected;
}

void TcpTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    state_ = TransportState::Disconnected;
    send_buffer_.clear();
    recv_buffer_.clear();
    peer_address_.clear();
}

}  // namespace chainled::net
