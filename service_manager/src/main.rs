mod device;
mod error;
mod service;
mod api;
mod websocket;

use service::ServiceManager;
use websocket::WebSocketServer;
use std::env;
use std::process::Command;
use std::io;
use tracing_subscriber;
use tracing::info;
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
async fn main() -> Result<(), Box<dyn std::error::Error>> {
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
    start_mqtt_broker()?;

    // Create event channel
    let (event_tx, event_rx) = tokio::sync::broadcast::channel(100);

    // Initialize service manager
    let service_manager = ServiceManager::new(true, event_tx)
        .map_err(|e| {
            eprintln!("Failed to initialize ServiceManager: {}", e);
            e
        })?;

    // Start WebSocket server
    let websocket_addr = "127.0.0.1:9001";
    tokio::spawn(async move {
        if let Err(e) = WebSocketServer::run(websocket_addr, event_rx).await {
            eprintln!("Failed to start WebSocket server: {}", e);
        }
    });

    // Start API server
    info!("Starting API server on http://127.0.0.1:3000");
    let listener = tokio::net::TcpListener::bind("127.0.0.1:3000").await?;
    
    // Serve API requests
    axum::serve(listener, api::create_router(service_manager)).await?;

    // Stop MQTT broker
    stop_mqtt_broker()?;

    Ok(())
}