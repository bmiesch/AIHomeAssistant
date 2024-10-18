#include "audio_capture.h"
#include "keyword_detector.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include "kernel.h"

std::atomic<bool> should_run(true);
Kernel* g_kernel = nullptr;

void signalHandler(int signum) {
   std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
   should_run = false;
   if (g_kernel) {
      g_kernel->Stop();
   }
}

int main() {
   std::signal(SIGINT, signalHandler);
   std::signal(SIGTERM, signalHandler);

   Kernel kernel;
   g_kernel = &kernel;
   
   kernel.Run();

   // Wait for a signal to stop
   while(should_run) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
   }

   std::cout << "Stopping kernel...\n";
   kernel.Stop();

   g_kernel = nullptr;
   std::cout << "Kernel stopped. Exiting.\n";
   return 0;
}