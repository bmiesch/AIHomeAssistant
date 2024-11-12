use std::collections::HashMap;
use std::time::Duration;
use thiserror::Error;
use ssh2::Session;
use std::net::TcpStream;
use std::path::Path;
use std::fs;
mod device;

use crate::device::{Device, DeviceRegistry, DeviceError, ROOT_DIR};
use paho_mqtt::{Error as MqttError, AsyncClient, CreateOptionsBuilder, ConnectOptionsBuilder};
use ssh2::Error as Ssh2Error;
use std::{
    io::{self, Write},
    env,
    process::Command,
};

//------------------------------------------------------------------------------
// Type Definitions
//------------------------------------------------------------------------------

#[derive(Debug)]
enum ServiceStatus {
    Created,
    Deployed,
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
    #[error("MQTT error: {0}")]
    MqttError(#[from] MqttError),
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    #[error("SSH2 error: {0}")]
    Ssh2Error(#[from] Ssh2Error),
    #[error("Deployment error: {0}")]
    DeploymentError(String),
    #[error("Error loading devices: {0}")]
    DeviceError(#[from] DeviceError),
    #[error("Service not found: {0}")]
    ServiceNotFound(String),
    #[error("Template error: {0}")]
    TemplateError(String),
}

//------------------------------------------------------------------------------
// Service Manager Implementation
//------------------------------------------------------------------------------

/// Manages the lifecycle of services across multiple devices
struct ServiceManager {
    services: HashMap<String, Service>,
    mqtt_client: paho_mqtt::AsyncClient,
    devices: DeviceRegistry,
}

// MQTT Implementations
impl ServiceManager {
    fn create_mqtt_client() -> Result<AsyncClient, ServiceManagerError> {
        let create_opts = CreateOptionsBuilder::new()
            .server_uri("tcp://localhost:1883")
            .client_id("service_manager")
            .finalize();
            
        Ok(AsyncClient::new(create_opts)?)
    }

    fn connect_mqtt_client(client: &AsyncClient) -> Result<(), ServiceManagerError> {
        let conn_opts = ConnectOptionsBuilder::new()
            .keep_alive_interval(Duration::from_secs(20))
            .clean_session(true)
            .finalize();
            
        client.connect(conn_opts).wait()?;
        Ok(())
    }
}

// Service Manager Helper Functions
impl ServiceManager {
    fn connect_ssh(device: &Device) -> Result<Session, ServiceManagerError> {
        let tcp = TcpStream::connect(&format!("{}:22", device.config.ip_address))?;
        let mut ssh = Session::new()?;
        ssh.set_tcp_stream(tcp);
        ssh.handshake()?;
        ssh.userauth_password(&device.config.username, &device.config.password)?;
        Ok(ssh)
    }

    fn execute_ssh_command(ssh: &Session, cmd: &str) -> Result<(), ServiceManagerError> {
        let mut channel = ssh.channel_session()?;
        channel.exec(cmd)?;
        channel.close()?;

        let exit_status = channel.exit_status()?;
        if exit_status != 0 {
            return Err(ServiceManagerError::DeploymentError(
                format!("Command failed with status {}: {}", exit_status, cmd)
            ));
        }
        
        Ok(())
    }

    fn read_service_template(service_name: &str) -> Result<String, ServiceManagerError> {
        let template_path = ROOT_DIR
            .join("services")
            .join(service_name)
            .join(format!("{}.service", service_name));
        
        fs::read_to_string(template_path)
            .map_err(|e| ServiceManagerError::TemplateError(format!("Failed to read template: {}", e)))
    }

    fn create_service_file(&self, service: &Service) -> Result<String, ServiceManagerError> {
        let template = Self::read_service_template(&service.name)?;
        Ok(template.replace("{username}", &service.device.config.username))
    }
}

// Service Manager Implementations
impl ServiceManager {
    /// Creates a new ServiceManager instance with MQTT connection
    pub fn new(load_devices: bool) -> Result<Self, ServiceManagerError> {
        let client = Self::create_mqtt_client()?;
        Self::connect_mqtt_client(&client)?;
            
        Ok(Self {
            services: HashMap::new(),
            mqtt_client: client,
            devices: DeviceRegistry::new(load_devices)?,
        })
    }

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
    fn deploy_service(&self, service_name: &str) -> Result<(), ServiceManagerError> {
        let service = self.services.get(service_name)
            .ok_or_else(|| ServiceManagerError::ServiceNotFound(
                service_name.to_string()
            ))?;

        println!("Deploying service: {}", service_name);
    
        println!("Cross-compiling service...");
        let script_path = ROOT_DIR.join("cross-compile");
        let status = std::process::Command::new("bash")
            .current_dir(&*ROOT_DIR)
            .arg(script_path)
            .args([
                &service.name,
                &service.device.config.arch,
            ])
            .env("ROOT_DIR", ROOT_DIR.to_str().unwrap())
            .status()
            .map_err(|e| ServiceManagerError::DeploymentError(e.to_string()))?;

        if !status.success() {
            return Err(ServiceManagerError::DeploymentError(
                format!("Cross-compilation failed with exit code: {}", status.code().unwrap_or(-1))
            ));
        }
    
        // Step 2: Connect to remote device
        let ssh = Self::connect_ssh(&service.device)?;
    
        // Step 3: Create service directory on remote device
        Self::execute_ssh_command(&ssh, "sudo mkdir -p /opt/services")?;
        Self::execute_ssh_command(&ssh, &format!("sudo chown {} /opt/services", service.device.config.username))?;
    
        // Step 4: Copy the binary to remote device
        let binary_path = ROOT_DIR.join("targets")
            .join(&service.device.config.arch)
            .join(format!("{}_service", service.name))
            .to_string_lossy()
            .to_string();
        let remote_path = format!("/opt/services/{}", service.name);

        // Verify binary exists locally
        if !std::path::Path::new(&binary_path).exists() {
            return Err(ServiceManagerError::DeploymentError(
                format!("Binary not found at: {}", binary_path)
            ));
        }

        // Copy file using scp command
        let status = Command::new("sshpass")
            .arg("-p")
            .arg(&service.device.config.password)
            .arg("scp")
            .arg(&binary_path)
            .arg(format!("{}@{}:{}", 
                service.device.config.username,
                service.device.config.ip_address,
                remote_path
            ))
            .status()?;

        if !status.success() {
            return Err(ServiceManagerError::DeploymentError(
                "Failed to copy binary to remote device".to_string()
            ));
        }

        // Set executable permissions
        Self::execute_ssh_command(&ssh, &format!("chmod 755 {}", remote_path))?;
            
        // Verify the file exists on remote
        Self::execute_ssh_command(&ssh, &format!("test -f {}", remote_path))?;
    
        // Step 5: Create systemd service file
        let service_content = self.create_service_file(service)?;
    
        // Step 6: Install and start the service
        Self::execute_ssh_command(&ssh, &format!(
            "echo '{}' | sudo tee /etc/systemd/system/{}.service",
            service_content, service.name
        ))?;
        
        Self::execute_ssh_command(&ssh, "sudo systemctl daemon-reload")?;
        Self::execute_ssh_command(&ssh, &format!("sudo systemctl enable {}", service.name))?;
        Self::execute_ssh_command(&ssh, &format!("sudo systemctl start {}", service.name))?;
    
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

//------------------------------------------------------------------------------
// Main Entry Point
//------------------------------------------------------------------------------

fn main() -> Result<(), ServiceManagerError> {

    // Start broker
    start_mqtt_broker()?;

    // Add a small delay to let the broker start up
    println!("Waiting for MQTT broker to start...");
    std::thread::sleep(Duration::from_secs(2));

    // Create service manager and load devices from yaml file
    let mut service_manager = ServiceManager::new(true)?;

    // let core_service = Service {
    //     name: "core".to_string(),
    //     status: ServiceStatus::Created,
    //     device: service_manager.devices.get_device("rpi02W").unwrap().clone(),
    // };

    let led_manager_service = Service {
        name: "led_manager".to_string(),
        status: ServiceStatus::Created,
        device: service_manager.devices.get_device("rpi02W").unwrap().clone(),
    };
    
    // Add service to service manager
    // service_manager.add_service(core_service);
    service_manager.add_service(led_manager_service);

    // Deploy service
    // service_manager.deploy_service("core")?;
    service_manager.deploy_service("led_manager")?;

    // Stop broker before exit
    stop_mqtt_broker()?;

    Ok(())
}