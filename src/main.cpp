#include "audio_capture.h"
#include "keyword_detector.h"

int main() {
   AudioCapture ac;
   KeywordDetector kd;

   // Capture audio
   std::cout << "Listening for 5 seconds..." << std::endl;
   auto buffer = ac.CaptureAudio(5000);

   // Print first 10 samples from buffer
   for (auto it = buffer.begin(); it != buffer.begin() + 10 && it != buffer.end(); ++it) {
      std::cout << *it << " ";
   }
   std::cout << std::endl;

   kd.DetectKeyword(buffer, true);
   
   return 0;
}