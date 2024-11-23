use std::collections::HashMap;
use std::time::Duration;
use ssh2::Session;
use std::net::TcpStream;
use std::fs;
use crate::device::{Device, DeviceRegistry, ROOT_DIR};
use paho_mqtt::{AsyncClient, CreateOptionsBuilder, ConnectOptionsBuilder, SslOptionsBuilder};
use std::process::Command;
use serde::Serialize;
use crate::error::*;
use tracing::{info, error};
use std::env;
use std::io::Read;

//------------------------s------------------------------------------------------
// Type Definitions
//------------------------------------------------------------------------------

#[derive(Debug, Serialize)]
pub enum ServiceStatus {
    Created,
    Deployed,
    Running,
    Stopped,
}

#[derive(Debug, Serialize)]
pub struct Service {
    pub name: String,
    pub status: ServiceStatus,
    pub device: Device,
}

//------------------------------------------------------------------------------
// Service Manager Implementation
//------------------------------------------------------------------------------

/// Manages the lifecycle of services across multiple devices
pub struct ServiceManager {
    services: HashMap<String, Service>,
    mqtt_client: paho_mqtt::AsyncClient,
    devices: DeviceRegistry,
}

// MQTT Implementations
impl ServiceManager {
    fn create_mqtt_client() -> Result<AsyncClient, ServiceManagerError> {
        let mqtt_broker = env::var("MQTT_BROKER").expect("MQTT_BROKER not set");
        let create_opts = CreateOptionsBuilder::new()
            .server_uri(mqtt_broker)
            .client_id("service_manager")
            .finalize();
            
        Ok(AsyncClient::new(create_opts)?)
    }

    fn connect_mqtt_client(client: &AsyncClient) -> Result<(), ServiceManagerError> {
        let mosquitto_path = env::var("SM_MOSQUITTO_DIR").expect("MOSQUITTO_PATH not set");
        let mqtt_username = env::var("MQTT_USERNAME").expect("MQTT_USERNAME not set");
        let mqtt_password = env::var("MQTT_PASSWORD").expect("MQTT_PASSWORD not set");

        let ssl_opts = SslOptionsBuilder::new()
            .trust_store(format!("{}/ca.crt", mosquitto_path))?
            .finalize();

        let conn_opts = ConnectOptionsBuilder::new()
            .keep_alive_interval(Duration::from_secs(20))
            .clean_session(true)
            .ssl_options(ssl_opts.clone())
            .user_name(mqtt_username)
            .password(mqtt_password)
            .finalize();

        client.connect(conn_opts).wait_for(Duration::from_secs(10))?;
        Ok(())
    }

    async fn subscribe_to_service(&mut self, service_name: &str) -> Result<(), ServiceManagerError> {
        let subscribe_token = self.mqtt_client.subscribe(service_name, 1);

        subscribe_token.wait()?;
        Ok(())
    }

    async fn subscribe_to_all_services(&mut self) -> Result<(), ServiceManagerError> {
        let subscribe_token = self.mqtt_client.subscribe("#", 1);
        subscribe_token.wait()?;
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

        let mut stdout = String::new();
        channel.read_to_string(&mut stdout)?;
        let mut stderr = String::new();
        channel.stderr().read_to_string(&mut stderr)?;

        channel.wait_close()?;
        let exit_status = channel.exit_status()?;

        if exit_status != 0 {
            return Err(ServiceManagerError::SshError(format!(
                "Command '{}' failed with status {}\nStderr: {}",
                cmd, exit_status, stderr
            )));
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

    fn create_service_file(service: &Service) -> Result<String, ServiceManagerError> {
        let mqtt_broker = env::var("MQTT_BROKER").expect("MQTT_BROKER not set");
        let mqtt_username = env::var("MQTT_USERNAME").expect("MQTT_USERNAME not set");
        let mqtt_password = env::var("MQTT_PASSWORD").expect("MQTT_PASSWORD not set");
        let mqtt_ca_dir = env::var("SERVICES_CA_DIR").expect("SERVICES_CA_DIR not set");

        let template = Self::read_service_template(&service.name)?;
        Ok(template.replace("{mqtt_broker}", &mqtt_broker)
            .replace("{mqtt_username}", &mqtt_username)
            .replace("{mqtt_password}", &mqtt_password)
            .replace("{mqtt_ca_dir}", &mqtt_ca_dir)
            .replace("{username}", &service.device.config.username))
    }

    fn get_service_mut(&mut self, service_name: &str) -> Result<&mut Service, ServiceManagerError> {
        self.services.get_mut(service_name)
            .ok_or_else(|| ServiceManagerError::ServiceError(
                ServiceError::ServiceNotFound(service_name.to_string())
            ))
    }
}

// Service Manager Implementations
impl ServiceManager {
    /// Creates a new ServiceManager instance with MQTT connection
    pub fn new(load_devices: bool) -> Result<Self, ServiceManagerError> {
        let client = Self::create_mqtt_client()?;
        info!("MQTT client created");
        Self::connect_mqtt_client(&client)?;
            
        Ok(Self {
            services: HashMap::new(),
            mqtt_client: client,
            devices: DeviceRegistry::new(load_devices)?,
        })
    }

    /// Creates a new service
    pub fn create_service(&mut self, service_name: String, device_name: String) -> Result<(), ServiceManagerError> {
        if self.services.contains_key(&service_name) {
            return Err(ServiceManagerError::ServiceError(
                ServiceError::ServiceAlreadyExists(service_name)
            ));
        }

        let device = self.devices.get_device(&device_name)?;
        let service = Service {
            name: service_name,
            status: ServiceStatus::Created,
            device: device.clone(),
        };
        self.services.insert(service.name.clone(), service);
        Ok(())
    }

    /// Returns a list of all services
    pub fn get_services(&self) -> Vec<&Service> {
        self.services.values().collect()
    }

    /// Removes a service from the manager
    pub fn remove_service(&mut self, service_name: &str) {
        // TODO: Remove the service from the remote device
        self.services.remove(service_name);
    }

    /// Deploy a service to a remote device
    pub async fn deploy_service(&mut self, service_name: &str) -> Result<(), ServiceManagerError> {
        let service = self.get_service_mut(service_name)?;
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
    
        // Copy the binary to remote device
        let ssh = Self::connect_ssh(&service.device)?;
        Self::execute_ssh_command(&ssh, "sudo mkdir -p /opt/services")?;
        Self::execute_ssh_command(&ssh, &format!("sudo chown {} /opt/services", service.device.config.username))?;
        let binary_path = ROOT_DIR.join("targets")
            .join(&service.device.config.arch)
            .join(format!("{}_service", service.name))
            .to_string_lossy()
            .to_string();
        let remote_path = format!("/opt/services/{}", service.name);
        if !std::path::Path::new(&binary_path).exists() {
            return Err(ServiceManagerError::DeploymentError(
                format!("Binary not found at: {}", binary_path)
            ));
        }

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
        Self::execute_ssh_command(&ssh, &format!("chmod 755 {}", remote_path))?;
        Self::execute_ssh_command(&ssh, &format!("test -f {}", remote_path))?;

        // Copy CA certificate to remote device
        let local_ca_cert_dir = env::var("SM_MOSQUITTO_DIR").expect("SM_MOSQUITTO_DIR not set");
        let local_ca_cert_path = format!("{}/ca.crt", local_ca_cert_dir);
        let remote_ca_dir = env::var("SERVICES_CA_DIR").expect("SERVICES_CA_DIR not set");
        let remote_ca_path = format!("{}/ca.crt", remote_ca_dir);
        Self::execute_ssh_command(&ssh, &format!("sudo rm -rf {}", remote_ca_dir))?;
        Self::execute_ssh_command(&ssh, &format!("sudo mkdir -p {}", remote_ca_dir))?;
        Self::execute_ssh_command(&ssh, &format!("sudo chown {} {}", service.device.config.username, remote_ca_dir))?;
        Self::execute_ssh_command(&ssh, &format!("sudo chmod 700 {}", remote_ca_dir))?;

        let status = Command::new("sshpass")
            .args([
                "-p", &service.device.config.password,
                "scp",
                &local_ca_cert_path,
                &format!("{}@{}:{}",
                    service.device.config.username,
                    service.device.config.ip_address,
                    remote_ca_path
                )
            ])
            .status()?;

        if !status.success() {
            return Err(ServiceManagerError::DeploymentError(
                "Failed to copy CA certificate".to_string()
            ));
        }
        Self::execute_ssh_command(&ssh, &format!("sudo chown {} {}", service.device.config.username, remote_ca_path))?;
        Self::execute_ssh_command(&ssh, &format!("sudo chmod 644 {}", remote_ca_path))?;

        // First disable and unmask the service if it exists
        info!("Cleaning up existing service state");
        Self::execute_ssh_command(&ssh, &format!(
            "sudo systemctl disable {} || true; \
            sudo systemctl unmask {} || true; \
            sudo rm -f /etc/systemd/system/{}",
            service_name, service_name, service_name
        ))?;

        let service_content = Self::create_service_file(service)?;
        Self::execute_ssh_command(&ssh, &format!(
            "echo '{}' | sudo tee /etc/systemd/system/{}.service",
            service_content, service.name
        ))?;
        
        Self::execute_ssh_command(&ssh, "sudo systemctl daemon-reload")?;
        // Self::execute_ssh_command(&ssh, &format!("sudo systemctl enable {}", service.name))?;
    
        println!("Service {} deployed successfully", service.name);
        service.status = ServiceStatus::Deployed;
        Ok(())
    }

    /// Start a service on a remote device by starting its systemd service
    pub async fn start_service(&mut self, service_name: &str) -> Result<(), ServiceManagerError> {
        let service = self.get_service_mut(service_name)?;

        // Check if service is already running
        if matches!(service.status, ServiceStatus::Running) {
            return Err(ServiceManagerError::ServiceError(
                ServiceError::ServiceStatusError(
                    "Service is already running".to_string()
                )
            ));
        }

        let ssh = Self::connect_ssh(&service.device)?;
        Self::execute_ssh_command(&ssh, &format!("sudo systemctl start {}", service.name))?;
        println!("Service {} started successfully", service.name);
        service.status = ServiceStatus::Running;

        // Subscribe to service topic
        self.subscribe_to_service(service_name).await?;
        Ok(())
    }

    /// Stop a service on a remote device by stopping its systemd service
    pub async fn stop_service(&mut self, service_name: &str) -> Result<(), ServiceManagerError> {
        let service = self.get_service_mut(service_name)?;

        // Check if service is already stopped
        if matches!(service.status, ServiceStatus::Stopped) {
            return Err(ServiceManagerError::ServiceError(
                ServiceError::ServiceStatusError(
                    "Service is already stopped".to_string()
                )
            ));
        }

        let ssh = Self::connect_ssh(&service.device)?;
        Self::execute_ssh_command(&ssh, &format!("sudo systemctl stop {}", service.name))?;
        println!("Service {} stopped successfully", service.name);
        service.status = ServiceStatus::Stopped;
        Ok(())
    }
}
