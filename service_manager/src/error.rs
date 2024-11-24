use std::io;
use thiserror::Error;
use tracing::error;
use paho_mqtt::Error as MqttError;
use ssh2::Error as Ssh2Error;
use serde_yaml;

#[derive(Debug, Error)]
pub enum ServiceError {
    #[error("Service not found: {0}")]
    ServiceNotFound(String),
    #[error("Service already exists: {0}")]
    ServiceAlreadyExists(String),
    #[error("Service status error: {0}")]
    ServiceStatusError(String),
}

#[derive(Debug, Error)]
pub enum DeviceError {
    #[error("Error loading devices: {0}")]
    IoError(io::Error),
    #[error("Error loading devices: {0}")]
    YamlError(serde_yaml::Error),
    #[error("Device not found: {0}")]
    DeviceNotFound(String),
    #[error("SSH2 error: {0}")]
    Ssh2Error(Ssh2Error),
    #[error("SSH error: {0}")]
    SshError(String),
}

#[derive(Debug, Error)]
pub enum ServiceManagerError {
    #[error("MQTT error: {0}")]
    MqttError(MqttError),
    #[error("IO error: {0}")]
    IoError(std::io::Error),
    #[error("Deployment error: {0}")]
    DeploymentError(String),
    #[error("Device error: {0}")]
    DeviceError(DeviceError),
    #[error("Template error: {0}")]
    TemplateError(String),
    #[error("Service error: {0}")]
    ServiceError(ServiceError),
    #[error("WebSocket error: {0}")]
    WebSocketError(String),
}

// Manual implementations with logging
impl From<io::Error> for DeviceError {
    fn from(err: io::Error) -> Self {
        error!(?err, "IO error occurred while loading devices");
        DeviceError::IoError(err)
    }
}

impl From<serde_yaml::Error> for DeviceError {
    fn from(err: serde_yaml::Error) -> Self {
        error!(?err, "YAML parsing error occurred");
        DeviceError::YamlError(err)
    }
}

impl From<Ssh2Error> for DeviceError {
    fn from(err: Ssh2Error) -> Self {
        error!(?err, "SSH error occurred");
        DeviceError::Ssh2Error(err)
    }
}

impl From<ServiceError> for ServiceManagerError {
    fn from(err: ServiceError) -> Self {
        error!(?err, "Service error occurred");
        ServiceManagerError::ServiceError(err)
    }
}

impl From<MqttError> for ServiceManagerError {
    fn from(err: MqttError) -> Self {
        error!(?err, "MQTT error occurred");
        ServiceManagerError::MqttError(err)
    }
}

impl From<std::io::Error> for ServiceManagerError {
    fn from(err: std::io::Error) -> Self {
        error!(?err, "IO error occurred");
        ServiceManagerError::IoError(err)
    }
}

impl From<DeviceError> for ServiceManagerError {
    fn from(err: DeviceError) -> Self {
        error!(?err, "Device error occurred");
        ServiceManagerError::DeviceError(err)
    }
}