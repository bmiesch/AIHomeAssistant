mod device;
mod service;
mod api;

use service::ServiceManager;
use std::env;
use std::process::Command;
use std::io;

//------------------------------------------------------------------------------
// MQTT Broker
//------------------------------------------------------------------------------
fn start_mqtt_broker() -> Result<(), io::Error> {
    match env::consts::OS {
        "macos" => {
            Command::new("brew")
                .args(["services", "start", "mosquitto"])
                .status()?;
        }
        "linux" => {
            Command::new("sudo")
                .args(["systemctl", "start", "mosquitto"])
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
    start_mqtt_broker().unwrap();

    // Your existing ServiceManager initialization
    let service_manager = ServiceManager::new(true)
        .expect("Failed to initialize service manager");

    // Add API server
    let app = api::create_router(service_manager);
    
    println!("Starting API server on http://127.0.0.1:3000");
    let listener = tokio::net::TcpListener::bind("127.0.0.1:3000")
        .await
        .unwrap();
    
    axum::serve(listener, app)
        .await
        .unwrap();

    stop_mqtt_broker().unwrap();
}