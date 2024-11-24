use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::{env, path::{Path, PathBuf}};
use once_cell::sync::Lazy;
use ssh2::Session;
use crate::error::*;
use std::fmt;
use std::net::TcpStream;
use std::io::Read;

pub static ROOT_DIR: Lazy<PathBuf> = Lazy::new(|| {
    PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap_or_else(|_| ".".to_string()))
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .to_path_buf()
});

#[derive(Serialize, Deserialize, Clone)]
pub struct Device {
    pub name: String,
    #[serde(flatten)]
    pub config: DeviceConfig,
    #[serde(skip_serializing, skip_deserializing)]
    pub ssh_session: Option<Session>,
}

impl fmt::Debug for Device {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Device")
            .field("name", &self.name)
            .field("config", &self.config)
            .finish()
    }
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
            ssh_session: None,
        }
    }

    fn is_ssh_session_valid(&self) -> bool {
        if let Some(ssh) = &self.ssh_session {
            if let Ok(mut channel) = ssh.channel_session() {
                if channel.exec("echo test").is_ok() {
                    if channel.wait_close().is_ok() {
                        return channel.exit_status().unwrap_or(1) == 0;
                    }
                }
            }
        }
        false
    }

    fn create_ssh_session(&mut self) -> Result<&Session, DeviceError> {
        let tcp = TcpStream::connect(&format!("{}:22", self.config.ip_address))?;
        let mut ssh = Session::new()?;
        ssh.set_tcp_stream(tcp);
        ssh.handshake()?;
        ssh.userauth_password(&self.config.username, &self.config.password)?;
        self.ssh_session = Some(ssh);
        Ok(&self.ssh_session.as_ref().unwrap())
    }

    fn get_ssh_session(&mut self) -> Result<&Session, DeviceError> {
        if self.ssh_session.is_none() || !self.is_ssh_session_valid() {
            return self.create_ssh_session();
        }

        Ok(&self.ssh_session.as_ref().unwrap())
    }

    pub fn execute_command(&mut self, cmd: &str) -> Result<(), DeviceError> {
        let ssh = self.get_ssh_session()?;

        let mut channel = ssh.channel_session()?;
        channel.exec(cmd)?;

        let mut stdout = String::new();
        channel.read_to_string(&mut stdout)?;
        let mut stderr = String::new();
        channel.stderr().read_to_string(&mut stderr)?;

        channel.wait_close()?;
        let exit_status = channel.exit_status()?;

        if exit_status != 0 {
            return Err(DeviceError::SshError(format!(
                "Command '{}' failed with status {}\nStderr: {}",
                cmd, exit_status, stderr
            )));
        }

        Ok(())
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