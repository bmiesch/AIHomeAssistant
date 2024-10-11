#include "voice_control.h"
#include <pocketsphinx.h>

int main() {
   AudioCapture ac;
   // auto buffer = ac.CaptureAudio(1000);
   // std::cout << "Captured " << buffer.size() << " samples" << std::endl;
   
   // for (auto sample : buffer) {
   //    std::cout << sample;
   // }
   // std::cout << std::endl;

   // Initialize PocketSphinx
   ps_decoder_t *ps;
   ps_config_t *config = ps_config_init(NULL);

   // Set up configuration
   ps_config_set_str(config, "hmm", "/usr/local/share/pocketsphinx/model/en-us/en-us");
   ps_config_set_str(config, "dict", "keyword.dict");  // Add this line
   ps_config_set_str(config, "kws", "keyword.list");
   ps_config_set_float(config, "kws_threshold", 1e-20);

   ps_config_set_bool(config, "verbose", true);

   ps = ps_init(config);
   if (ps == nullptr) {
      throw std::runtime_error("Failed to initialize PocketSphinx");
   }

   // Capture audio
   std::cout << "Listening for 5 seconds..." << std::endl;
   auto buffer = ac.CaptureAudio(5000);  // Capture 5 seconds of audio

   // Print first 10 samples from buffer
   for (auto it = buffer.begin(); it != buffer.begin() + 10 && it != buffer.end(); ++it) {
      std::cout << *it << " ";
   }
   std::cout << std::endl;

   // Perform speech recognition
   std::cout << "Recognizing..." << std::endl;
   ps_start_utt(ps);
   ps_process_raw(ps, buffer.data(), buffer.size(), false, false);
   ps_end_utt(ps);

   // Get and print the recognition result
   const char* hyp = ps_get_hyp(ps, NULL);
   if (hyp != NULL) {
      std::cout << "Detected keyword: " << hyp << std::endl;
   } else {
      std::cout << "No keyword detected." << std::endl;
   }

   // Clean up
   ps_free(ps);
   ps_config_free(config);

   return 0;
}