#include "server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>

HttpServer::HttpServer(int port, MetricsState& state, const std::string& web_root) 
    : server_fd_(-1), port_(port), web_root_(web_root), shared_state_(state) {
    
    // Игнорируем SIGPIPE, иначе сервер упадет, если клиент разорвет соединение во время отправки данных
    signal(SIGPIPE, SIG_IGN);

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) throw std::runtime_error("Не удалось создать сокет");

    // Позволяем сразу переиспользовать порт после перезапуска приложения
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Ошибка привязки (bind) к порту. Возможно, он занят.");
    }
    if (listen(server_fd_, 10) < 0) {
        throw std::runtime_error("Ошибка listen");
    }
}

HttpServer::~HttpServer() { 
    stop(); 
}

void HttpServer::start() {
    is_running_ = true;
    std::cout << "Сервер запущен. Откройте в браузере: http://localhost:" << port_ << "\n";

    while (is_running_) {
        int client_socket = accept(server_fd_, nullptr, nullptr);
        if (!is_running_) {
            if (client_socket >= 0) close(client_socket);
            break;
        }

        if (client_socket >= 0) {
            // Устанавливаем таймаут на чтение
            struct timeval tv{5, 0};
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

            // Отпускаем поток в свободное плавание (detach)
            std::thread(&HttpServer::handleClient, this, client_socket).detach();
        }
    }
}

void HttpServer::stop() {
    if (!is_running_) return;
    is_running_ = false;
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR); // Принудительно будим поток accept()
        close(server_fd_);
        server_fd_ = -1;
    }
}

void HttpServer::handleClient(int client_socket) {
    char buffer[2048] = {0};
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    
    // Если ничего не прочитали или клиент отвалился - просто уходим
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }

    // Вытаскиваем метод и путь из первой строки HTTP-запроса (например: "GET /metrics HTTP/1.1")
    std::istringstream request_stream(buffer);
    std::string method, path, version;
    request_stream >> method >> path >> version;

    if (method != "GET") {
        sendHttpResponse(client_socket, "405 Method Not Allowed", "text/plain", "Only GET allowed");
        close(client_socket);
        return;
    }

    // Отбрасываем параметры запроса (всё, что после '?')
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) {
        path = path.substr(0, qpos);
    }

    if (path == "/metrics" || path == "/metrics/") {
        handleSseConnection(client_socket);
        return; // сокет закроется внутри handleSseConnection при отключении клиента
    } 
    else if (path == "/") {
        serveStaticFile(client_socket, "index.html", "text/html");
    } 
    else if (path == "/style.css") {
        serveStaticFile(client_socket, "style.css", "text/css");
    } 
    else if (path == "/app.js") {
        serveStaticFile(client_socket, "app.js", "application/javascript");
    } 
    else {
        sendHttpResponse(client_socket, "404 Not Found", "text/plain", "Page Not Found");
    }
    
    close(client_socket);
}

void HttpServer::serveStaticFile(int client_socket, const std::string& filename, const std::string& content_type) {
    std::string full_path = web_root_ + "/" + filename;
    std::ifstream file(full_path, std::ios::binary);
    
    if (!file.is_open()) {
        sendHttpResponse(client_socket, "404 Not Found", "text/plain", "File Not Found: " + filename);
        return;
    }

    // Считываем весь файл в память так как простой фронтентд
    std::ostringstream ss;
    ss << file.rdbuf();
    sendHttpResponse(client_socket, "200 OK", content_type, ss.str());
}

void HttpServer::sendHttpResponse(int client_socket, const std::string& status, const std::string& content_type, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.length() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;
             
    std::string resp_str = response.str();
    send(client_socket, resp_str.c_str(), resp_str.length(), MSG_NOSIGNAL);
}

void HttpServer::handleSseConnection(int client_socket) {
    // Отправляем заголовки для Server-Sent Events
    std::string headers = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n\r\n";
    
    if (send(client_socket, headers.c_str(), headers.length(), MSG_NOSIGNAL) < 0) {
        close(client_socket); 
        return;
    }

    // Бесконечный цикл отправки данных. Если клиент закроет вкладку, send() вернет ошибку и мы выйдем из цикла.
    while (is_running_) {
        SystemMetrics metrics = shared_state_.get();
        std::string payload = "data: " + metrics.toJson() + "\n\n";

        if (send(client_socket, payload.c_str(), payload.length(), MSG_NOSIGNAL) < 0) {
            break; // Клиент отключился
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    close(client_socket);
}
