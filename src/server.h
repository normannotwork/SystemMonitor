#pragma once
#include "shared_state.h"
#include <string>
#include <atomic>

class HttpServer {
  private:
      int server_fd_;
      int port_;
      std::string web_root_;
      std::atomic<bool> is_running_{false};
      MetricsState& shared_state_;
      
  public:
      HttpServer(int port, MetricsState& state, const std::string& web_root);
      ~HttpServer();
      
      void start();
      void stop();

  private:
      void handleClient(int client_socket);
      void handleSseConnection(int client_socket);
      void serveStaticFile(int client_socket, const std::string& filename, const std::string& content_type);
      void sendHttpResponse(int client_socket, const std::string& status, const std::string& content_type, const std::string& body);
};
