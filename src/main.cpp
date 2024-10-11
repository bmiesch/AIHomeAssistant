#include "voice_control.h"


int main() {
   AudioCapture ac;
   auto buffer = ac.CaptureAudio(1000);
   std::cout << "Captured " << buffer.size() << " samples" << std::endl;
   
   for (auto sample : buffer) {
      std::cout << sample;
   }
   std::cout << std::endl;

   return 0;
}