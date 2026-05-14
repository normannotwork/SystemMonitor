#pragma once
#include <string>
#include <shared_mutex>
#include <sstream>
#include <mutex>

// Храним все метрики системы в одной структуре
struct SystemMetrics {
    double cpu_usage_percent = 0.0;
    unsigned long long ram_total_mb = 0;
    unsigned long long ram_used_mb = 0;
    double load_avg_1m = 0.0;
    unsigned long long net_rx_kbps = 0; 
    unsigned long long net_tx_kbps = 0;

    // Cборка JSON
    std::string toJson() const {
        std::ostringstream oss;
        oss << "{"
            << "\"cpu\":" << cpu_usage_percent << ","
            << "\"ram_total\":" << ram_total_mb << ","
            << "\"ram_used\":" << ram_used_mb << ","
            << "\"load\":" << load_avg_1m << ","
            << "\"net_rx\":" << net_rx_kbps << ","
            << "\"net_tx\":" << net_tx_kbps
            << "}";
        return oss.str();
    }
};

// Класс для безопасной передачи данных между потоком-сборщиком и веб-сервером
class MetricsState {
  private:
      SystemMetrics metrics_;
      mutable std::shared_mutex mutex_;
  public:
      void set(const SystemMetrics& m) {
          // Пока пишем, никто не читает
          std::unique_lock lock(mutex_);
          metrics_ = m;
      }

      SystemMetrics get() const {
          // Читать могут сразу несколько клиентов параллельно
          std::shared_lock lock(mutex_);
          return metrics_;
      }
};
