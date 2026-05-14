#include "monitor.h"
#include <fstream>
#include <sstream>

Monitor::Monitor(MetricsState& state) : shared_state_(state) {
    prev_time_ = std::chrono::steady_clock::now();
}

Monitor::~Monitor() { 
    stop(); 
}

void Monitor::start() {
    if (is_running_) return;
    is_running_ = true;
    worker_thread_ = std::thread(&Monitor::run, this);
}

void Monitor::stop() {
    is_running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void Monitor::run() {
    SystemMetrics metrics;
    
    // Делаем начальный прогон, чтобы заполнить prev_ переменные начальными данными
    updateCpuUsage(metrics);
    updateNetworkUsage(metrics);

    // Основной цикл сборщика
    while (is_running_) {
        updateCpuUsage(metrics);
        updateRamUsage(metrics);
        updateLoadAvg(metrics);
        updateNetworkUsage(metrics);
        
        shared_state_.set(metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Обновляем дважды в секунду
    }
}

void Monitor::updateCpuUsage(SystemMetrics& metrics) {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) return;

    std::string cpu_label;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    
    // Читаем первую строчку (общая статистика по всем ядрам)
    if (stat_file >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
        unsigned long long idle_time = idle + iowait;
        unsigned long long total_time = user + nice + system + idle + iowait + irq + softirq + steal;

        // Защита от деления на ноль на случай аномалий в системе
        if (prev_total_time_ != 0 && total_time > prev_total_time_) {
            unsigned long long total_delta = total_time - prev_total_time_;
            unsigned long long idle_delta = idle_time - prev_idle_time_;
            // 100% минус процент простоя
            metrics.cpu_usage_percent = 100.0 * (1.0 - ((double)idle_delta / total_delta));
        }
        
        prev_idle_time_ = idle_time;
        prev_total_time_ = total_time;
    }
}

void Monitor::updateRamUsage(SystemMetrics& metrics) {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return;

    std::string key;
    unsigned long long value;
    std::string unit;
    unsigned long long mem_total = 0, mem_available = 0;

    // Ищем нужные ключи, игнорируем остальное
    while (meminfo >> key >> value >> unit) {
        if (key == "MemTotal:") mem_total = value;
        else if (key == "MemAvailable:") { 
            mem_available = value; 
            break; // Нашли всё что нужно, дальше не читаем
        }
    }

    metrics.ram_total_mb = mem_total / 1024;
    if (mem_total >= mem_available) {
        metrics.ram_used_mb = (mem_total - mem_available) / 1024;
    }
}

void Monitor::updateLoadAvg(SystemMetrics& metrics) {
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg.is_open()) {
        loadavg >> metrics.load_avg_1m; // Забираем только показатель за 1 минуту
    }
}

void Monitor::updateNetworkUsage(SystemMetrics& metrics) {
    std::ifstream dev_file("/proc/net/dev");
    if (!dev_file.is_open()) return;

    std::string line;
    // Пропускаем первые две строки с заголовками таблицы
    std::getline(dev_file, line); 
    std::getline(dev_file, line); 

    unsigned long long total_rx = 0, total_tx = 0;
    while (std::getline(dev_file, line)) {
        std::istringstream iss(line);
        std::string iface;
        iss >> iface;
        
        // Локальную петлю не считаем за сетевой трафик
        if (iface == "lo:") continue;

        unsigned long long rx, tx, dummy;
        // Нам нужны 1-е (rx) и 9-е (tx) значения после имени интерфейса
        iss >> rx >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> tx;
        total_rx += rx;
        total_tx += tx;
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = now - prev_time_;

    // Проверяем diff, чтобы избежать деления на ноль при слишком быстром вызове
    if (prev_net_rx_ != 0 && diff.count() > 0.001) {
        metrics.net_rx_kbps = ((total_rx - prev_net_rx_) / 1024.0) / diff.count();
        metrics.net_tx_kbps = ((total_tx - prev_net_tx_) / 1024.0) / diff.count();
    }

    prev_net_rx_ = total_rx;
    prev_net_tx_ = total_tx;
    prev_time_ = now;
}
