#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
// Windows sockets
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
static const socket_t INVALID_SOCKET_T = INVALID_SOCKET;
#else
// POSIX sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
using socket_t = int;
static const socket_t INVALID_SOCKET_T = -1;
#endif

#include "../include/lru_cache.h"
#include "../include/distributed_cache.h"

// ---------- helpers ----------
static void trim_crlf(std::string &s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
}

static std::vector<std::string> split_ws(const std::string &line)
{
    std::istringstream iss(line);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok)
        out.push_back(tok);
    return out;
}

static bool starts_with(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin());
}

static void send_all(socket_t sock, const std::string &data)
{
    const char *buf = data.c_str();
    size_t total = 0;
    size_t len = data.size();
    while (total < len)
    {
#ifdef _WIN32
        int sent = ::send(sock, buf + total, static_cast<int>(len - total), 0);
#else
        ssize_t sent = ::send(sock, buf + total, len - total, 0);
#endif
        if (sent <= 0)
            return;
        total += static_cast<size_t>(sent);
    }
}

static void close_socket(socket_t sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

// ---------- command handling ----------
// protocol (line-based):
//   HEALTH
//   STATS
//   SINGLE PUT <key> <value...>
//   SINGLE GET <key>
//   SINGLE DEL <key>
//   NODE <nodeId> PUT <key> <value...>
//   NODE <nodeId> GET <key>
//   NODE <nodeId> DEL <key>
//
// responses (one-line):
//   OK
//   OK VALUE <value...>
//   OK MISS
//   OK REMOVED
//   OK NOT_FOUND
//   ERR <message>

static std::string join_rest(const std::vector<std::string> &toks, size_t start)
{
    std::string out;
    for (size_t i = start; i < toks.size(); i++)
    {
        if (i > start)
            out += " ";
        out += toks[i];
    }
    return out;
}

static std::string handle_single(LRUCache &single_cache, const std::vector<std::string> &toks)
{
    // toks: ["SINGLE", op, ...]
    if (toks.size() < 3)
        return "ERR SINGLE requires op and key\n";

    const std::string &op = toks[1];
    const std::string &key = toks[2];

    if (op == "PUT")
    {
        if (toks.size() < 4)
            return "ERR PUT requires value\n";
        std::string value = join_rest(toks, 3);
        single_cache.put(key, value);
        return "OK\n";
    }
    else if (op == "GET")
    {
        try
        {
            std::string value = single_cache.get(key);
            return "OK VALUE " + value + "\n";
        }
        catch (const std::out_of_range &)
        {
            return "OK MISS\n";
        }
    }
    else if (op == "DEL")
    {
        bool removed = single_cache.erase(key);
        return removed ? "OK REMOVED\n" : "OK NOT_FOUND\n";
    }

    return "ERR unknown SINGLE op\n";
}

static std::string handle_node(DistributedCache &dist_cache, const std::vector<std::string> &toks)
{
    // toks: ["NODE", nodeId, op, key, value...]
    if (toks.size() < 4)
        return "ERR NODE requires nodeId and op\n";

    const std::string &nodeId = toks[1];
    const std::string &op = toks[2];

    auto node = dist_cache.getNode(nodeId);
    if (!node)
        return "ERR unknown nodeId\n";

    if (toks.size() < 5 && (op == "PUT"))
        return "ERR PUT requires key and value\n";

    if (toks.size() < 4 + 1)
        ; // handled below

    if (op == "PUT")
    {
        if (toks.size() < 5)
            return "ERR PUT requires key and value\n";
        const std::string &key = toks[3];
        std::string value = join_rest(toks, 4);

        // Primary write: write to chosen node, invalidate peers (already done in putToNode)
        dist_cache.putToNode(nodeId, key, value);
        return "OK\n";
    }
    else if (op == "GET")
    {
        if (toks.size() < 4)
            return "ERR GET requires key\n";
        const std::string &key = toks[3];
        try
        {
            std::string value = node->get(key);
            return "OK VALUE " + value + "\n";
        }
        catch (const std::out_of_range &)
        {
            return "OK MISS\n";
        }
    }
    else if (op == "DEL")
    {
        if (toks.size() < 4)
            return "ERR DEL requires key\n";
        const std::string &key = toks[3];

        bool removed = dist_cache.delFromNode(nodeId, key);
        return removed ? "OK REMOVED\n" : "OK NOT_FOUND\n";
    }

    return "ERR unknown NODE op\n";
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    const int PORT = 9090;

    // caches
    LRUCache single_cache(1000);

    DistributedCache dist_cache;
    dist_cache.addNode("node1", 1000);
    dist_cache.addNode("node2", 1000);
    dist_cache.addNode("node3", 1000);

    socket_t server_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET_T)
    {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
#ifdef _WIN32
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
#else
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
#endif

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (::bind(server_sock, (sockaddr *)&addr, sizeof(addr)) != 0)
    {
        std::cerr << "bind() failed (is port 9090 in use?)\n";
        close_socket(server_sock);
        return 1;
    }

    if (::listen(server_sock, 16) != 0)
    {
        std::cerr << "listen() failed\n";
        close_socket(server_sock);
        return 1;
    }

    std::cout << "C++ cache server listening on 127.0.0.1:" << PORT << std::endl;

    while (true)
    {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif
        socket_t client = ::accept(server_sock, (sockaddr *)&client_addr, &client_len);
        if (client == INVALID_SOCKET_T)
            continue;

        // handle one client sequentially (simple)
        std::string buf_accum;
        char buf[4096];

        while (true)
        {
#ifdef _WIN32
            int n = recv(client, buf, sizeof(buf), 0);
#else
            ssize_t n = recv(client, buf, sizeof(buf), 0);
#endif
            if (n <= 0)
                break;

            buf_accum.append(buf, buf + n);

            // process lines
            size_t pos;
            while ((pos = buf_accum.find('\n')) != std::string::npos)
            {
                std::string line = buf_accum.substr(0, pos + 1);
                buf_accum.erase(0, pos + 1);
                trim_crlf(line);

                if (line.empty())
                    continue;

                std::vector<std::string> toks = split_ws(line);
                if (toks.empty())
                    continue;

                std::string resp;

                if (toks[0] == "HEALTH")
                {
                    resp = "OK\n";
                }
                else if (toks[0] == "STATS")
                {
                    // Keep it minimal and machine-friendly
                    // Format: OK SINGLE_SIZE <n> NODE1_SIZE <n> NODE2_SIZE <n> NODE3_SIZE <n>
                    auto n1 = dist_cache.getNode("node1");
                    auto n2 = dist_cache.getNode("node2");
                    auto n3 = dist_cache.getNode("node3");
                    int s1 = n1 ? n1->getCacheSize() : 0;
                    int s2 = n2 ? n2->getCacheSize() : 0;
                    int s3 = n3 ? n3->getCacheSize() : 0;

                    resp = "OK SINGLE_SIZE " + std::to_string(single_cache.size()) +
                           " NODE1_SIZE " + std::to_string(s1) +
                           " NODE2_SIZE " + std::to_string(s2) +
                           " NODE3_SIZE " + std::to_string(s3) + "\n";
                }
                else if (toks[0] == "SINGLE")
                {
                    resp = handle_single(single_cache, toks);
                }
                else if (toks[0] == "NODE")
                {
                    // NOTE: DELETE for NODE not implemented in this first pass.
                    // We'll implement it cleanly in next step by adding a delete method on CacheNode/DistributedCache.
                    resp = handle_node(dist_cache, toks);
                }
                else
                {
                    resp = "ERR unknown command\n";
                }

                send_all(client, resp);
            }
        }

        close_socket(client);
    }

    // unreachable
    close_socket(server_sock);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}