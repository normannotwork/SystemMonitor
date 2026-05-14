#include "server.h"
#include "monitor.h"
#include "shared_state.h"
#include <iostream>
#include <csignal>

// Глобальный указатель для корректного завершения по Ctrl+C
HttpServer* global_server = nullptr;

void signalHandler(int signum) {
    if (global_server) {
        std::cout << "\nПолучен сигнал завершения (" << signum << "). Останавливаем сервер..." << std::endl;
        global_server->stop();
    }
}

int main() {
    try {
        MetricsState state;
        
        // Запускаем сборщик метрик в фоне
        Monitor monitor(state);
        monitor.start(); 

        // Инициализируем сервер. Укажи правильный путь до папки с фронтендом!
        HttpServer server(8080, state, "../frontend/dist");
        global_server = &server;

        // Перехватываем Ctrl+C (SIGINT) и команду kill (SIGTERM)
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // Этот метод заблокирует поток, пока сервер работает
        server.start(); 
        
        // Сюда попадем только после вызова stop() (например, по Ctrl+C)
        monitor.stop(); 
        std::cout << "Программа успешно завершена." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
