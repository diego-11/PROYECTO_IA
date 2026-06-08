#include "board.hpp"
#include "alphabeta.hpp"
#include "mcts.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ── Business logic ────────────────────────────────────────────────────────────
static std::string processRequest(const std::string& body) {
    try {
        auto req = json::parse(body);

        std::array<int,14> pits;
        auto boardArr = req.at("board");
        if (boardArr.size() != 14)
            return "{\"error\":\"board must have 14 values\"}";
        for (int i = 0; i < 14; ++i) pits[i] = boardArr[i].get<int>();

        int  side    = req.at("side").get<int>();
        auto algo    = req.value("algo",        std::string("alphabeta"));
        int  depth   = req.value("depth",       8);
        int  sims    = req.value("simulations", 10000);
        int  threads = req.value("threads",     1);
        bool leafP   = req.value("leaf_parallel", false);

        Board board(pits, side);

        if (board.isTerminal())
            return "{\"error\":\"board is already terminal\"}";

        json resp;

        if (algo == "alphabeta") {
            ABResult r = alphaBetaRoot(board, depth, threads, threads == 1);
            if (r.bestMove < 0)
                return "{\"error\":\"no legal moves\"}";
            resp["move"]         = r.bestMove;
            resp["evaluation"]   = r.bestVal;
            resp["elapsed_ms"]   = (int)(r.stats.elapsed * 1000);
            resp["threads_used"] = r.stats.threads;
            resp["stats"] = {
                {"algo",   "alphabeta"},
                {"nodes",  r.stats.nodes},
                {"prunes", r.stats.prunes}
            };
        } else if (algo == "mcts") {
            MCTSResult r = mcts(board, sims, threads, leafP);
            if (r.bestMove < 0)
                return "{\"error\":\"no legal moves\"}";
            resp["move"]         = r.bestMove;
            resp["evaluation"]   = r.winRate;
            resp["elapsed_ms"]   = (int)(r.stats.elapsed * 1000);
            resp["threads_used"] = r.stats.threads;
            resp["stats"] = {
                {"algo",           "mcts"},
                {"rollouts",       r.stats.rollouts},
                {"tree_depth_avg", r.stats.treeDepthN > 0
                    ? r.stats.treeDepthSum / r.stats.treeDepthN : 0.0},
                {"win_rate",       r.winRate}
            };
        } else {
            return "{\"error\":\"unknown algo, use alphabeta or mcts\"}";
        }

        return resp.dump();
    } catch (const std::exception& e) {
        json err;
        err["error"] = e.what();
        return err.dump();
    }
}

// ── Robust HTTP request reader ────────────────────────────────────────────────
// Reads headers then reads exactly Content-Length bytes of body.
static bool readAll(int fd, char* buf, int n) {
    int total = 0;
    while (total < n) {
        int r = recv(fd, buf + total, n - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

static bool handleClient(int client) {
    // Read headers (until \r\n\r\n), up to 8 KB
    std::string headers;
    headers.reserve(2048);
    char c;
    while (true) {
        int r = recv(client, &c, 1, 0);
        if (r <= 0) return false;
        headers += c;
        if (headers.size() >= 4 &&
            headers.substr(headers.size()-4) == "\r\n\r\n")
            break;
        if (headers.size() > 8192) return false; // header too large
    }

    // Parse first line: METHOD PATH HTTP/x.y
    std::istringstream hs(headers);
    std::string method, path;
    hs >> method >> path;

    // Parse Content-Length
    int contentLength = 0;
    {
        std::string line;
        // re-parse headers line by line
        std::istringstream hs2(headers);
        while (std::getline(hs2, line)) {
            // tolower comparison
            std::string low = line;
            for (auto& ch : low) ch = tolower(ch);
            if (low.find("content-length:") == 0) {
                contentLength = std::stoi(line.substr(15));
                break;
            }
        }
    }

    // Build response
    std::string respBody;
    int statusCode = 200;
    std::string statusText = "OK";

    if (path == "/healthz" || path == "/readyz") {
        respBody = "{\"status\":\"ok\"}";
    } else if (path == "/move" && method == "POST") {
        if (contentLength <= 0) {
            respBody    = "{\"error\":\"missing Content-Length\"}";
            statusCode  = 400;
            statusText  = "Bad Request";
        } else if (contentLength > 1048576) {
            respBody    = "{\"error\":\"body too large\"}";
            statusCode  = 413;
            statusText  = "Payload Too Large";
        } else {
            std::string body(contentLength, '\0');
            if (!readAll(client, body.data(), contentLength)) {
                respBody   = "{\"error\":\"read error\"}";
                statusCode = 500;
                statusText = "Internal Server Error";
            } else {
                respBody = processRequest(body);
            }
        }
    } else {
        respBody   = "{\"error\":\"not found\"}";
        statusCode = 404;
        statusText = "Not Found";
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: application/json; charset=utf-8\r\n";
    response << "Content-Length: " << respBody.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << respBody;

    std::string rstr = response.str();
    send(client, rstr.c_str(), (int)rstr.size(), 0);
    return true;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    int port = 8001;
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0)
            port = atoi(argv[i+1]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(server_fd, 32);

    std::cerr << "Motor HTTP server listening on port " << port << std::endl;

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;
        handleClient(client);
        close(client);
    }

    close(server_fd);
    return 0;
}
