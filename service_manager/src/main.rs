use std::collections::HashMap;
use std::time::Duration;
use thiserror::Error;
use std::env;
use ssh2::Session;
use std::net::TcpStream;
use std::io::{Error, ErrorKind};
use std::path::Path;
use std::io::Write;

//------------------------------------------------------------------------------
// Type Definitions
//------------------------------------------------------------------------------

#[derive(Debug)]
struct Device {
    name: String,
    ip_address: String,
    username: String,
    password: String,  // TODO: Consider using a more secure type
}

#[derive(Debug)]
enum ServiceStatus {
    Created,
    Running,
    Stopped,
}

#[derive(Debug)]
struct Service {
    name: String,
    status: ServiceStatus,
    device: Device,
}

//------------------------------------------------------------------------------
// Error Handling
//------------------------------------------------------------------------------

#[derive(Debug, Error)]
pub enum ServiceManagerError {
    #[error("Error creating MQTT client: {0}")]
    MqttCreationError(String),
    #[error("Error connecting to MQTT broker: {0}")]
    MqttConnectionError(String),
    #[error("Error with MQTT broker service: {0}")]
    MqttBrokerError(String),
}

//------------------------------------------------------------------------------
// Service Manager Implementation
//------------------------------------------------------------------------------

/// Manages the lifecycle of services across multiple devices
struct ServiceManager {
    services: HashMap<String, Service>,
    mqtt_client: paho_mqtt::AsyncClient,
}

// MQTT-related implementations
impl ServiceManager {
    /// Creates a new ServiceManager instance with MQTT connection
    pub fn new() -> Result<Self, ServiceManagerError> {
        let client = Self::create_mqtt_client()?;
        Self::connect_mqtt_client(&client)?;
        
        Ok(Self {
            services: HashMap::new(),
            mqtt_client: client,
        })
    }

    fn create_mqtt_client() -> Result<paho_mqtt::AsyncClient, ServiceManagerError> {
        let create_opts = paho_mqtt::CreateOptionsBuilder::new()
            .server_uri("tcp://localhost:1883")
            .client_id("service_manager")
            .finalize();
            
        paho_mqtt::AsyncClient::new(create_opts)
            .map_err(|e| ServiceManagerError::MqttCreationError(e.to_string()))
    }

    fn connect_mqtt_client(client: &paho_mqtt::AsyncClient) -> Result<(), ServiceManagerError> {
        let conn_opts = paho_mqtt::ConnectOptionsBuilder::new()
            .keep_alive_interval(Duration::from_secs(20))
            .clean_session(true)
            .connect_timeout(Duration::from_secs(5))
            .automatic_reconnect(Duration::from_secs(1), Duration::from_secs(60))
            .finalize();
            
        client.connect(conn_opts)
            .wait()
            .map(|_| ())
            .map_err(|e| ServiceManagerError::MqttConnectionError(e.to_string()))
    }
}

// Service management implementations
impl ServiceManager {
    /// Adds a new service to the manager
    fn add_service(&mut self, service: Service) {
        self.services.insert(service.name.clone(), service);
    }

    /// Removes a service from the manager
    fn remove_service(&mut self, service_name: &str) {
        self.services.remove(service_name);
    }

    /// Deploy a service to a remote device
    /// 
    /// # Steps
    /// - Install dependencies
    /// - Compile binary and copy to remote device  
    /// - Create systemd service
    fn deploy_service(&self, service: &Service) -> Result<(), std::io::Error> {
        println!("Deploying service: {}", service.name);
    
        // Step 1: Cross-compile using our existing Dockerfile
        println!("Cross-compiling service...");
        let status = std::process::Command::new("docker")
            .args([
                "build",
                "--build-arg", &format!("SERVICE_NAME={}", service.name),
                "-t", "service-compiler",
                "-f", ".docker/compile/Dockerfile",
                "."
            ])
            .status()?;
    
        if !status.success() {
            return Err(Error::new(ErrorKind::Other, "Cross-compilation failed"));
        }
    
        // Step 2: Connect to remote device
        let tcp = TcpStream::connect(&format!("{}:22", service.device.ip_address))?;
        let mut ssh = Session::new().map_err(|e| Error::new(ErrorKind::Other, e.to_string()))?;
        ssh.set_tcp_stream(tcp);
        ssh.handshake()?;
        ssh.userauth_password(&service.device.username, &service.device.password)?;
    
        // Helper function for remote commands
        let execute = |cmd: &str| -> Result<(), std::io::Error> {
            let mut channel = ssh.channel_session()?;
            channel.exec(cmd)?;
            channel.wait_close()?;
            if channel.exit_status()? != 0 {
                return Err(Error::new(ErrorKind::Other, format!("Command failed: {}", cmd)));
            }
            Ok(())
        };
    
        // Step 3: Create service directory on remote device
        execute("sudo mkdir -p /opt/services")?;
    
        // Step 4: Copy the binary to remote device
        let binary_path = format!("target/release/{}", service.name);
        let remote_path = format!("/opt/services/{}", service.name);
        
        let mut channel = ssh.scp_send(
            Path::new(&remote_path),  // Convert to Path
            0o755,
            std::fs::metadata(&binary_path)?.len(),
            None
        )?;
        
        let binary_data = std::fs::read(&binary_path)?;
        channel.write_all(&binary_data)?;  // Use write_all instead of write
        channel.send_eof()?;
        channel.wait_eof()?;
        channel.close()?;
    
        // Step 5: Create systemd service file
        let service_content = format!(
            r#"[Unit]
    Description={}
    
    [Service]
    ExecStart=/opt/services/{}
    Restart=always
    User={}
    
    [Install]
    WantedBy=multi-user.target"#,
            service.name,
            service.name,
            service.device.username
        );
    
        // Step 6: Install and start the service
        execute(&format!(
            "echo '{}' | sudo tee /etc/systemd/system/{}.service",
            service_content, service.name
        ))?;
        execute("sudo systemctl daemon-reload")?;
        execute(&format!("sudo systemctl enable {}", service.name))?;
        execute(&format!("sudo systemctl start {}", service.name))?;
    
        println!("Service {} deployed successfully", service.name);
        Ok(())
    }

    /// Start a service on a remote device by starting its systemd service
    fn start_service(&self, service: &Service) -> Result<(), std::io::Error> {
        println!("Starting service: {}", service.name);
        Ok(())
    }

    /// Stop a service on a remote device by stopping its systemd service
    fn stop_service(&self, service: &Service) -> Result<(), std::io::Error> {
        println!("Stopping service: {}", service.name);
        Ok(())
    }
}

//------------------------------------------------------------------------------
// MQTT Broker
//------------------------------------------------------------------------------
fn start_mqtt_broker() -> Result<(), ServiceManagerError> {
    match env::consts::OS {
        "macos" => {
            std::process::Command::new("brew")
                .args(["services", "start", "mosquitto"])
                .status()
                .map_err(|e| ServiceManagerError::MqttBrokerError(e.to_string()))?;
        }
        "linux" => {
            std::process::Command::new("sudo")
                .args(["systemctl", "start", "mosquitto"])
                .status()
                .map_err(|e| ServiceManagerError::MqttBrokerError(e.to_string()))?;
        }
        os => {
            return Err(ServiceManagerError::MqttBrokerError(
                format!("Unsupported operating system: {}", os)
            ));
        }
    }
    Ok(())
}

fn stop_mqtt_broker() -> Result<(), ServiceManagerError> {
    match env::consts::OS {
        "macos" => {
            std::process::Command::new("brew")
                .args(["services", "stop", "mosquitto"])
                .status()
                .map_err(|e| ServiceManagerError::MqttBrokerError(e.to_string()))?;
        }
        "linux" => {
            std::process::Command::new("sudo")
                .args(["systemctl", "stop", "mosquitto"])
                .status()
                .map_err(|e| ServiceManagerError::MqttBrokerError(e.to_string()))?;
        }
        os => {
            return Err(ServiceManagerError::MqttBrokerError(
                format!("Unsupported operating system: {}", os)
            ));
        }
    }
    Ok(())
}

//------------------------------------------------------------------------------
// Main Entry Point
//------------------------------------------------------------------------------

fn main() -> Result<(), ServiceManagerError> {

    // Start broker
    start_mqtt_broker()?;

    // Add a small delay to let the broker start up
    println!("Waiting for MQTT broker to start...");
    std::thread::sleep(Duration::from_secs(2));

    // Create service manager
    let mut service_manager = ServiceManager::new()?;

    // Add example service
    service_manager.add_service(Service {
        name: "core".to_string(),
        status: ServiceStatus::Created,
        device: Device {
            name: "rpi02W".to_string(),
            ip_address: "192.168.0.148".to_string(),
            username: "bmiesch".to_string(),
            password: "".to_string(),
        },
    });

    // Stop broker before exit
    stop_mqtt_broker()?;

    Ok(())
}