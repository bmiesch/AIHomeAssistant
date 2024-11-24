use tokio::net::TcpListener;
use tokio_tungstenite::accept_async;
use futures_util::{SinkExt, StreamExt};
use tokio::sync::broadcast;
use crate::error::ServiceManagerError;

pub struct WebSocketServer;

impl WebSocketServer {
    pub async fn run(addr: &str, event_rx: broadcast::Receiver<String>) -> Result<(), ServiceManagerError> {
        let listener = TcpListener::bind(addr).await
            .map_err(|e| ServiceManagerError::WebSocketError(e.to_string()))?;
        tracing::info!("WebSocket server listening on {}", addr);

        while let Ok((stream, addr)) = listener.accept().await {
            tracing::debug!("New WebSocket connection from {}", addr);
            let ws_stream = accept_async(stream).await
                .map_err(|e| ServiceManagerError::WebSocketError(e.to_string()))?;
            let (mut write, read) = ws_stream.split();
            let mut client_rx = event_rx.resubscribe();

            // Handle incoming messages
            tokio::spawn(async move {
                read.for_each(|message| async {
                    match message {
                        Ok(msg) => {
                            tracing::debug!("Received WebSocket message from {}: {:?}", addr, msg);
                        }
                        Err(e) => {
                            tracing::error!("WebSocket error from {}: {}", addr, e);
                        }
                    }
                }).await;
            });

            // Handle outgoing messages
            tokio::spawn(async move {
                while let Ok(msg) = client_rx.recv().await {
                    if let Err(e) = write.send(tokio_tungstenite::tungstenite::Message::Text(msg)).await {
                        tracing::error!("Failed to send WebSocket message: {}", e);
                        break;
                    }
                }
            });
        }

        Ok(())
    }
}