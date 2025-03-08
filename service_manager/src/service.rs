use std::collections::HashMap;
use std::fs;
use crate::device::{Device, DeviceRegistry, ROOT_DIR};
use std::process::Command;
use serde::Serialize;
use crate::error::*;
use tracing::{error, info};
use std::env;
use tokio::sync::broadcast;
use crate::mqtt_client::MQTTClient;

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
    pub enabled: bool,
    pub device: Device,
}

//------------------------------------------------------------------------------
// Service Manager Implementation
//------------------------------------------------------------------------------

/// Manages the lifecycle of services across multiple devices
pub struct ServiceManager {
    services: HashMap<String, Service>,
    pub mqtt_client: MQTTClient,
    devices: DeviceRegistry,
}


// Service Manager Helper Functions
impl ServiceManager {
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
        let picovoice_access_key = env::var("PICOVOICE_ACCESS_KEY").expect("PICOVOICE_ACCESS_KEY not set");

        let template = Self::read_service_template(&service.name)?;
        Ok(template.replace("{mqtt_broker}", &mqtt_broker)
            .replace("{mqtt_username}", &mqtt_username)
            .replace("{mqtt_password}", &mqtt_password)
            .replace("{mqtt_ca_dir}", &mqtt_ca_dir)
            .replace("{picovoice_access_key}", &picovoice_access_key)
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
    pub fn new(load_devices: bool, event_tx: broadcast::Sender<String>) -> Result<Self, ServiceManagerError> {
        Ok(Self {
            services: HashMap::new(),
            mqtt_client: MQTTClient::new(&event_tx)?,
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
            enabled: false,
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
        info!("Deploying service: {}", service_name);

        info!("Cross-compiling service...");
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
        service.device.execute_command("sudo mkdir -p /opt/services")?;
        service.device.execute_command(&format!("sudo chown {} /opt/services", service.device.config.username))?;

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
        service.device.execute_command(&format!("chmod 755 {}", remote_path))?;
        service.device.execute_command(&format!("test -f {}", remote_path))?;

        // Copy CA certificate to remote device
        let local_ca_cert_dir = env::var("SM_MOSQUITTO_DIR").expect("SM_MOSQUITTO_DIR not set");
        let local_ca_cert_path = format!("{}/ca.crt", local_ca_cert_dir);
        let remote_ca_dir = env::var("SERVICES_CA_DIR").expect("SERVICES_CA_DIR not set");
        let remote_ca_path = format!("{}/ca.crt", remote_ca_dir);
        service.device.execute_command(&format!("sudo rm -rf {}", remote_ca_dir))?;
        service.device.execute_command(&format!("sudo mkdir -p {}", remote_ca_dir))?;
        service.device.execute_command(&format!("sudo chown {} {}", service.device.config.username, remote_ca_dir))?;
        service.device.execute_command(&format!("sudo chmod 700 {}", remote_ca_dir))?;

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
        service.device.execute_command(&format!("sudo chown {} {}", service.device.config.username, remote_ca_path))?;
        service.device.execute_command(&format!("sudo chmod 644 {}", remote_ca_path))?;

        // Copy over the lib files
        let local_lib_dir = ROOT_DIR.join("services").join(&service.name).join("lib");
        let remote_lib_dir = format!("/usr/local/lib/{}", service.name);
        service.device.execute_command(&format!("sudo rm -rf {}", remote_lib_dir))?;
        service.device.execute_command(&format!("sudo mkdir -p {}", remote_lib_dir))?;
        service.device.execute_command(&format!("sudo chown {} {}", service.device.config.username, remote_lib_dir))?;

        if local_lib_dir.exists() {
            let status = Command::new("sshpass")
                .args([
                    "-p", &service.device.config.password,
                    "scp",
                    "-r",
                    &format!("{}/.", local_lib_dir.to_str().unwrap()),
                    &format!("{}@{}:{}",
                        service.device.config.username,
                        service.device.config.ip_address,
                        remote_lib_dir
                    )   
                ])
                .status()?;

            if !status.success() {
                return Err(ServiceManagerError::DeploymentError(
                    "Failed to copy library files".to_string()
                ));
            }
        }

        // Copy over the models
        let local_models_dir = ROOT_DIR.join("services").join(&service.name).join("models");
        let remote_models_dir = format!("/usr/local/lib/{}", service.name);
        service.device.execute_command(&format!("sudo rm -rf {}", remote_models_dir))?;
        service.device.execute_command(&format!("sudo mkdir -p {}", remote_models_dir))?;
        service.device.execute_command(&format!("sudo chown {} {}", service.device.config.username, remote_models_dir))?;

        if local_models_dir.exists() {
            let status = Command::new("sshpass")
                .args([
                    "-p", &service.device.config.password,
                    "scp",
                    "-r",
                    &format!("{}/.", local_models_dir.to_str().unwrap()),
                    &format!("{}@{}:{}",
                        service.device.config.username,
                        service.device.config.ip_address,
                        remote_models_dir
                    )
                ])
                .status()?;

            if !status.success() {
                return Err(ServiceManagerError::DeploymentError(
                    "Failed to copy models".to_string()
                ));
            }
        }

        // First disable and unmask the service if it exists
        info!("Cleaning up existing service state");
        service.device.execute_command(&format!(
            "sudo systemctl disable {} || true; \
            sudo systemctl unmask {} || true; \
            sudo rm -f /etc/systemd/system/{}",
            service_name, service_name, service_name
        ))?;

        let service_content = Self::create_service_file(service)?;
        service.device.execute_command(&format!(
            "echo '{}' | sudo tee /etc/systemd/system/{}.service",
            service_content, service.name
        ))?;
        
        service.device.execute_command("sudo systemctl daemon-reload")?;

        info!("Service {} deployed successfully", service.name);
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

        service.device.execute_command(&format!("sudo systemctl start {}", service.name))?;
        info!("Service {} started successfully", service.name);
        service.status = ServiceStatus::Running;
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

        service.device.execute_command(&format!("sudo systemctl stop {}", service.name))?;
        info!("Service {} stopped successfully", service.name);
        service.status = ServiceStatus::Stopped;
        Ok(())
    }

    /// Enable a service on a remote device by enabling its systemd service
    pub async fn enable_service(&mut self, service_name: &str) -> Result<(), ServiceManagerError> {
        let service = self.get_service_mut(service_name)?;
        service.device.execute_command(&format!("sudo systemctl enable {}", service.name))?;
        service.enabled = true;
        Ok(())
    }

    /// Disable a service on a remote device by disabling its systemd service
    pub async fn disable_service(&mut self, service_name: &str) -> Result<(), ServiceManagerError> {
        let service = self.get_service_mut(service_name)?;
        service.device.execute_command(&format!("sudo systemctl disable {}", service.name))?;
        service.enabled = false;
        Ok(())
    }
}