mod device;
mod error;
mod service;
mod api;

use service::ServiceManager;
use std::env;
use std::process::Command;
use std::io;
use tracing_subscriber;
use dotenv::dotenv;

//------------------------------------------------------------------------------
// MQTT Broker default: port 1883
//------------------------------------------------------------------------------
fn start_mqtt_broker() -> Result<(), io::Error> {
    dotenv().ok();
    let mosquitto_path = env::var("MOSQUITTO_PATH").unwrap();

    match env::consts::OS {
        "macos" => {
            // Check if Mosquitto is already running
            let status_output = Command::new("brew")
                .args(["services", "list"])
                .output()?;

            let status_output_str = String::from_utf8_lossy(&status_output.stdout);

            if status_output_str.contains("mosquitto") && status_output_str.contains("started") {
                Command::new("brew")
                    .args(["services", "restart", "mosquitto"])
                    .status()?;
            } else {
                Command::new("brew")
                    .args(["services", "start", "mosquitto"])
                    .status()?;
            }
        }
        "linux" => {
            Command::new("sudo")
                .args(["systemctl", "start", "mosquitto", "--", "-c", &mosquitto_path])
                .status()?;
        }
        os => {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                format!("Unsupported operating system: {}", os)
            ));
        }
    }
    Ok(())
}

fn stop_mqtt_broker() -> Result<(), io::Error> {
    match env::consts::OS {
        "macos" => {
            Command::new("brew")
                .args(["services", "stop", "mosquitto"])
                .status()?;
        }
        "linux" => {
            Command::new("sudo")
                .args(["systemctl", "stop", "mosquitto"])
                .status()?;
        }
        os => {
            return Err(io::Error::new(
                io::ErrorKind::Unsupported,
                format!("Unsupported operating system: {}", os)
            ));
        }
    }
    Ok(())
}

#[tokio::main]
async fn main() {

    tracing_subscriber::fmt()
        .with_target(false)      // Don't include the target (module path)
        .with_thread_ids(true)   // Include thread IDs for async debugging
        .with_level(true)        // Include log level (INFO, ERROR, etc)
        .with_file(true)         // Include file name
        .with_line_number(true)  // Include line number
        .pretty()                // Use pretty formatting (more readable)
        .init();

    start_mqtt_broker().unwrap();

    // Your existing ServiceManager initialization
    let service_manager = ServiceManager::new(true)
        .expect("Failed to initialize service manager");
    
    println!("Starting API server on http://127.0.0.1:3000");
    let listener = tokio::net::TcpListener::bind("127.0.0.1:3000")
        .await
        .unwrap();
    
    axum::serve(listener, api::create_router(service_manager))
        .await
        .unwrap();

    stop_mqtt_broker().unwrap();
}