use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::io;
use thiserror::Error;
use std::{env, path::{Path, PathBuf}};
use once_cell::sync::Lazy;

pub static ROOT_DIR: Lazy<PathBuf> = Lazy::new(|| {
    PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap_or_else(|_| ".".to_string()))
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .to_path_buf()
});

#[derive(Debug, Error)]
pub enum DeviceError {
    #[error("Error loading devices: {0}")]
    IoError(#[from] io::Error),
    #[error("Error loading devices: {0}")]
    YamlError(#[from] serde_yaml::Error),
    #[error("Device not found: {0}")]
    DeviceNotFound(String),
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Device {
    pub name: String,
    #[serde(flatten)]
    pub config: DeviceConfig,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct DeviceConfig {
    pub arch: String,
    pub ip_address: String,
    pub username: String,
    #[serde(skip_serializing)]
    pub password: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DeviceRegistry {
    pub devices: HashMap<String, Device>,
}

impl Device {
    pub fn new(name: String, arch: String, ip: String, username: String, password: String) -> Self {
        Self {
            name,
            config: DeviceConfig {
                arch,
                ip_address: ip,
                username,
                password,
            },
        }
    }
}

impl DeviceRegistry {
    pub fn new(load_from_file: bool) -> Result<Self, DeviceError> {
        if load_from_file {
            println!("Root dir: {}", ROOT_DIR.display());
            let config_path = ROOT_DIR.join("service_manager/config/devices.yaml");
            let config_content = std::fs::read_to_string(config_path)?;
            let registry: DeviceRegistry = serde_yaml::from_str(&config_content)?;
            Ok(registry)
        } else {
            Ok(Self {
                devices: HashMap::new(),
            })
        }
    }

    pub fn add_device(&mut self, device: Device) {
        self.devices.insert(device.name.clone(), device);
    }

    pub fn get_device(&self, device_name: &str) -> Result<&Device, DeviceError> {
        self.devices
            .get(device_name)
            .ok_or(DeviceError::DeviceNotFound(device_name.to_string()))
    }
}