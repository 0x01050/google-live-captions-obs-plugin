/******************************************************************************
Copyright (C) 2019 by <rat.with.a.compiler@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "TcpConnection.h"

#include "utils.h"
#include "log.h"

#define READ_BUFFER_SIZE 2048

#include "ip_utils.c"

#define SOCKET_RECV_BUFFER_SIZE 4096
#define SOCKET_SEND_BUFFER_SIZE 1024

volatile bool is_setup = false;

static void setup_check() {
    if (is_setup)
        return;

    info_log("p_libsys_init");
    p_libsys_init();
    is_setup = true;
}


TcpConnection::TcpConnection(string hostname, uint port)
        : hostname(hostname), port(port) {
    setup_check();
}

void TcpConnection::connect(uint timeoutMs) {
    if (started)
        throw ConnectError("connection already started, wuttt");

    started = true;

    const char *ip_str = host_to_ip(hostname.c_str());

    if (ip_str == nullptr)
        throw ConnectError("couldn't resolve hostname");

    ip_address = ip_str;

    if ((p_address = p_socket_address_new(ip_address.c_str(), port)) == nullptr)
        throw ConnectError("couldn't create address");

    if ((p_socket = p_socket_new(P_SOCKET_FAMILY_INET, P_SOCKET_TYPE_STREAM, P_SOCKET_PROTOCOL_TCP, nullptr)) == nullptr) {
        throw ConnectError("couldn't create p_socket");
    }

    if (timeoutMs) {
        p_socket_set_timeout(p_socket, timeoutMs);
    }

    if (!p_socket_connect(p_socket, p_address, nullptr)) {
        throw ConnectError("couldn't connect to server");
    }

    connected = true;
}


bool TcpConnection::is_connected() {
    return connected;
}

bool TcpConnection::is_dead() {
    return dead;
}

void TcpConnection::close() {
    if (p_socket != nullptr) {
        p_socket_close(p_socket, nullptr);
        p_socket = nullptr;
    }

    if (p_address != nullptr) {
        p_socket_address_free(p_address);
        p_address = nullptr;
    }
}

TcpConnection::~TcpConnection() {
    close();
}

void TcpConnection::set_timeout(uint timeoutMs) {
    if (!p_socket)
        return;

    p_socket_set_timeout(p_socket, timeoutMs);
}

bool TcpConnection::send_all(const char *buffer, const int bytes) {
    if (!p_socket || !bytes)
        return false;

    int left = bytes;
    while (left > 0) {
        int just_sent = p_socket_send(p_socket, buffer + bytes - left, left, nullptr);
        if (just_sent == -1)
            return false;

        left -= just_sent;
    }
    return true;
}

int TcpConnection::receive_at_most(char *buffer, int bytes) {
    return p_socket_receive(p_socket, buffer, bytes, nullptr);
}

int TcpConnection::receive_exactly(string &buffer, int bytes) {
    int read = 0;
    int cur_read = 0;
    char chunk[READ_BUFFER_SIZE];
    while (read < bytes) {
        int needed = bytes - read;
        if ((cur_read = p_socket_receive(p_socket, chunk, needed, nullptr)) == -1)
            return -1;

        read += cur_read;
        buffer.append(chunk, cur_read);
    }

    return read;
}

int TcpConnection::receive_at_least(string &buffer, int bytes) {
    int read = 0;
    int cur_read = 0;
    char chunk[READ_BUFFER_SIZE];
    while (read < bytes) {
        if ((cur_read = p_socket_receive(p_socket, chunk, READ_BUFFER_SIZE, nullptr)) == -1)
            return -1;

        read += cur_read;
        buffer.append(chunk, cur_read);
    }

    return read;
}