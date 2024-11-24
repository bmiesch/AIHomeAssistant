use serde_json;

#[derive(Debug)]
pub enum MessageType {
    Status,
    Command,
    Unknown(String),
}

fn parse_topic(topic: &str) -> (String, MessageType) {
    let parts: Vec<&str> = topic.split('/').collect();
    
    // Extract service name (usually the second-to-last part)
    let service_name = parts.iter()
        .rev()
        .nth(1)
        .unwrap_or(&"unknown")
        .to_string();

    // Determine message type from the last part
    let msg_type = match parts.last() {
        Some(&"status") => MessageType::Status,
        Some(&"command") => MessageType::Command,
        Some(other) => MessageType::Unknown(other.to_string()),
        None => MessageType::Unknown("".to_string()),
    };

    (service_name, msg_type)
}

pub fn format_message(message: &str) -> String {
    // Extract topic and content from the message
    if let Some(topic_start) = message.find("topic '") {
        if let Some(topic_end) = message[topic_start..].find("': ") {
            let topic = &message[topic_start + 7..topic_start + topic_end];
            let content = &message[topic_start + topic_end + 3..];

            let (service_name, msg_type) = parse_topic(topic);

            match msg_type {
                MessageType::Status => {
                    if let Ok(json) = serde_json::from_str::<serde_json::Value>(content) {
                        return format!("{} | Status: {}", 
                            service_name,
                            json["status"].as_str().unwrap_or("unknown")
                        );
                    }
                },
                MessageType::Command => {
                    return format!("{} | Command received", service_name);
                },
                MessageType::Unknown(t) => {
                    return format!("{} | {}: {}", service_name, t, content.trim());
                }
            }
        }
    }
    
    message.to_string()
}