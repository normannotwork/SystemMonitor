#pragma once
#include "shared_state.h"
#include <atomic>
#include <thread>
#include <chrono>

class Monitor {
  private:
      MetricsState& shared_state_;
      std::atomic<bool> is_running_{false};
      std::thread worker_thread_;

      // Сохраняем предыдущие значения для вычисления разницы
      // Это нужно для процессора и сети, так как в Linux лежат абсолютные счетчики
      unsigned long long prev_idle_time_ = 0;
      unsigned long long prev_total_time_ = 0;
      unsigned long long prev_net_rx_ = 0;
      unsigned long long prev_net_tx_ = 0;
      std::chrono::steady_clock::time_point prev_time_;
      
  public:
      Monitor(MetricsState& state);
      ~Monitor();
      
      void start();
      void stop();

  private:
      void run();
      void updateCpuUsage(SystemMetrics& metrics);
      void updateRamUsage(SystemMetrics& metrics);
      void updateLoadAvg(SystemMetrics& metrics);
      void updateNetworkUsage(SystemMetrics& metrics);
};
