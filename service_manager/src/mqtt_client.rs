use paho_mqtt::{AsyncClient, CreateOptionsBuilder, ConnectOptionsBuilder, SslOptionsBuilder, Message};
use std::env;
use tokio::sync::broadcast;
use std::time::Duration;
use tracing::info;

#[derive(Clone)]
pub struct MQTTClient {
    client: paho_mqtt::AsyncClient,
}


impl MQTTClient {
    pub fn new(event_tx: &broadcast::Sender<String>) -> Result<Self, paho_mqtt::Error> {
        let client = Self::create_client(event_tx)?;

        // Connect the client
        Self::connect_client(&client)?;

        // Subscribe to all services
        Self::subscribe_to_all_services(&client)?;

        Ok(Self { client })
    }


    fn create_client(event_tx: &broadcast::Sender<String>) -> Result<AsyncClient, paho_mqtt::Error> {
        let mqtt_broker = env::var("MQTT_BROKER").expect("MQTT_BROKER not set");
        let create_opts = CreateOptionsBuilder::new()
            .server_uri(mqtt_broker)
            .client_id("service_manager")
            .finalize();

        let client = AsyncClient::new(create_opts)?;

        // Set up message callback
        let event_tx = event_tx.clone();
        client.set_message_callback(move |_cli, msg| {
            if let Some(msg) = msg {
                let message = format!(
                    "Received message on topic '{}': {}", 
                    msg.topic(), 
                    msg.payload_str()
                );
                if let Err(e) = event_tx.send(message) {
                    tracing::error!("Failed to send MQTT message to event bus: {}", e);
                }
            }
        });
        
        Ok(client)
    }

    fn connect_client(client: &AsyncClient) -> Result<(), paho_mqtt::Error> {
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
            .will_message(Message::new("home/services/service_manager/status", "offline", 1))
            .finalize();

        client.connect(conn_opts).wait_for(Duration::from_secs(10))?;
        Ok(())
    }

    pub async fn subscribe_to_service(&mut self, service_name: &str) -> Result<(), paho_mqtt::Error> {
        let subscribe_token = self.client.subscribe(service_name, 1);
        subscribe_token.wait()?;
        Ok(())
    }

    pub fn subscribe_to_all_services(client: &AsyncClient) -> Result<(), paho_mqtt::Error> {
        let subscribe_token = client.subscribe("#", 1);
        subscribe_token.wait()?;
        Ok(())
    }

    pub async fn publish(self, msg: &Message) -> Result<(), paho_mqtt::Error> {
        let publish_token = self.client.publish(msg.clone());
        publish_token.wait()?;
        Ok(())
    }
}