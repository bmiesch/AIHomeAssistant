#include "manager.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

// Helper function to get user input
int getUserInput() {
   int choice;
   std::cin >> choice;
   return choice;
}

int main() {
   try {
      // Define device configurations
      std::vector<DeviceConfig> configs = {
         {"BE:67:00:AC:C8:82", "0000fff0-0000-1000-8000-00805f9b34fb", "0000fff3-0000-1000-8000-00805f9b34fb"},
         {"BE:67:00:6A:B5:A6", "0000fff0-0000-1000-8000-00805f9b34fb", "0000fff3-0000-1000-8000-00805f9b34fb"}
         // Add more device configurations as needed
      };

      // Initialize the BLE Manager
      BLEManager manager(configs);

      std::cout << "BLE Manager initialized successfully.\n";

      while (true) {
         std::cout << "\nChoose an action:\n"
                     << "1. Turn on all devices\n"
                     << "2. Turn off all devices\n"
                     << "3. Set color for all devices\n"
                     << "4. Exit\n"
                     << "Enter your choice: ";

         int choice = getUserInput();

         switch (choice) {
               case 1:
                  manager.TurnOnDevices();
                  std::cout << "All devices turned on.\n";
                  break;
               case 2:
                  manager.TurnOffDevices();
                  std::cout << "All devices turned off.\n";
                  break;
               case 3:
                  {
                     int r, g, b;
                     std::cout << "Enter RGB values (0-255) separated by commas (e.g., 255,128,0): ";
                     std::string input;
                     std::getline(std::cin >> std::ws, input);
                     
                     std::istringstream iss(input);
                     char comma;
                     if (iss >> r >> comma >> g >> comma >> b) {
                           if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                              manager.SetDevicesColor(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
                              std::cout << "Color set for all devices.\n";
                           } else {
                              std::cout << "Invalid input. RGB values must be between 0 and 255.\n";
                           }
                     } else {
                           std::cout << "Invalid input format. Please use comma-separated values.\n";
                     }
                  }
                  break;
               case 4:
                  std::cout << "Exiting program.\n";
                  return 0;
               default:
                  std::cout << "Invalid choice. Please try again.\n";
         }

         // Small delay to prevent console spam
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
   }
   catch (const std::exception& e) {
      std::cerr << "An error occurred: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}