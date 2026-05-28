#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "store.h"
#include <thread>

#define PORT 6379

// Split "SET name alice" into ["SET", "name", "alice"]
std::vector<std::string> splitCmd(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string w;
    while (ss >> w)
        out.push_back(w);
    return out;
}

// Run one command, return the response string
std::string handleCommand(Store &db, const std::string &raw)
{
    auto p = splitCmd(raw);
    if (p.empty())
        return "";

    std::string cmd = p[0];
    for (auto &c : cmd)
        c = toupper(c); // case-insensitive

    if (cmd == "PING")
        return "+PONG\r\n";

    if (cmd == "SET")
    {
        if (p.size() < 3)
            return "-ERR wrong args\r\n";
        int ttl = -1;
        if (p.size() >= 5)
        {
            std::string ex = p[3];
            for (auto &c : ex)
                c = toupper(c);
            if (ex == "EX")
                ttl = std::stoi(p[4]);
        }
        db.set(p[1], p[2], ttl);
        return "+OK\r\n";
    }

    if (cmd == "GET")
    {
        if (p.size() < 2)
            return "-ERR wrong args\r\n";
        std::string val = db.get(p[1]);
        if (val == "(nil)")
            return "$-1\r\n"; // nil bulk string
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    }

    if (cmd == "DEL")
    {
        if (p.size() < 2)
            return "-ERR wrong args\r\n";
        return ":" + std::to_string(db.del(p[1])) + "\r\n";
    }

    if (cmd == "DBSIZE")
        return ":" + std::to_string(db.size()) + "\r\n";

    return "-ERR unknown command '" + p[0] + "'\r\n";
}

// Handle one connected client until they disconnect
void handleClient(int client_fd, Store &db)
{
    char buf[1024];
    std::string leftover; // holds incomplete data between recv() calls

    while (true)
    {
        int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break; // 0 = clean disconnect, -1 = error
        buf[n] = '\0';
        leftover += buf;

        // Process every complete line (commands end with \n)
        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos)
        {
            std::string line = leftover.substr(0, pos);
            leftover = leftover.substr(pos + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;

            std::string resp = handleCommand(db, line);
            if (!resp.empty())
                send(client_fd, resp.c_str(), resp.size(), 0);
        }
    }
}

int main()
{
    Store db;

    // Step 1: Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    // Step 2: Bind to port 6379
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    // Step 3: Start listening
    listen(server_fd, 10);
    std::cout << "Mini Redis listening on port " << PORT << "...\n";

    // Step 4 + 5: Accept clients and handle their commands
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            continue;

        // Spawn a new thread for this client — it runs handleClient independently
        std::thread([client_fd, &db]()
                    {
        handleClient(client_fd, db);
        close(client_fd); })
            .detach(); // detach = thread runs on its own, we don't wait for it
    }

    close(server_fd);
    return 0;
}