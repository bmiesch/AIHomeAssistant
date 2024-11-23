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
    let mosquitto_path = env::var("SM_MOSQUITTO_DIR").expect("SM_MOSQUITTO_DIR not set");

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
    // Initialize logging
    tracing_subscriber::fmt()
        .with_target(false)
        .with_thread_ids(true)
        .with_level(true)
        .with_file(true)
        .with_line_number(true)
        .pretty()
        .init();

    // Load environment variables
    dotenv().ok();

    // Start MQTT broker
    start_mqtt_broker().unwrap();

    // Initialize service manager
    let service_manager = ServiceManager::new(true)
        .expect("Failed to initialize service manager");
    
    // Start API server
    println!("Starting API server on http://127.0.0.1:3000");
    let listener = tokio::net::TcpListener::bind("127.0.0.1:3000")
        .await
        .unwrap();
    
    // Serve API requests
    axum::serve(listener, api::create_router(service_manager))
        .await
        .unwrap();

    // Stop MQTT broker
    stop_mqtt_broker().unwrap();
}