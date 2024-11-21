use axum::{
    Router,
    routing::{get, post, delete},
    extract::{State, Path},
    Json,
};
use std::sync::Arc;
use tokio::sync::Mutex;

use crate::service::ServiceManager;
use serde::{Deserialize, Serialize};
use tower_http::cors::{CorsLayer, Any};
use crate::error::*;

#[derive(Deserialize)]
pub struct CreateServiceRequest {
    device_name: String,
}

#[derive(Serialize, Debug)]
pub struct ServiceResponse {
    name: String,
    status: String,
    device: String,
}


//TODO: Handle errors better. Maybe the API should have its own error type.
impl axum::response::IntoResponse for ServiceManagerError {
    fn into_response(self) -> axum::response::Response {
        (
            axum::http::StatusCode::INTERNAL_SERVER_ERROR,
            self.to_string(),
        ).into_response()
    }
}

// Shared state type
pub type SharedState = Arc<Mutex<ServiceManager>>;

// Create router with endpoints
pub fn create_router(service_manager: ServiceManager) -> Router {
    let shared_state = Arc::new(Mutex::new(service_manager));

    // Create a permissive CORS middleware
    let cors = CorsLayer::new()
        .allow_origin(Any)
        .allow_methods(Any)
        .allow_headers(Any);

    Router::new()
        .route("/services", get(list_services))
        .route("/services/:name", post(create_service))
        .route("/services/:name/remove", delete(remove_service))
        .route("/services/:name/deploy", post(deploy_service))
        .route("/services/:name/start", post(start_service))
        .route("/services/:name/stop", post(stop_service))
        .with_state(shared_state)
        .layer(cors)
}

/// List all services
async fn list_services(
    State(state): State<SharedState>,
) -> Json<Vec<ServiceResponse>> {
    println!("Listing services");
    let service_manager = state.lock().await;
    let services = service_manager.get_services()
        .iter()
        .map(|s| ServiceResponse {
            name: s.name.clone(),
            status: format!("{:?}", s.status),
            device: s.device.config.ip_address.clone(),
        })
        .collect();
    
    Json(services)
}

/// Create a new service
async fn create_service(
    State(state): State<SharedState>,
    Path(name): Path<String>,
    Json(req): Json<CreateServiceRequest>,
) -> Result<Json<String>, ServiceManagerError> {
    println!("Creating service");
    let mut service_manager = state.lock().await;
    match service_manager.create_service(name, req.device_name) {
        Ok(_) => Ok(Json("Service created".to_string())),
        Err(e) => Err(e)
    }
}

/// Remove a service
async fn remove_service(
    State(state): State<SharedState>,
    Path(name): Path<String>,
) -> Result<Json<String>, ServiceManagerError> {
    println!("Removing service");
    let mut service_manager = state.lock().await;
    service_manager.remove_service(&name);
    Ok(Json("Service removed".to_string()))
}

/// Deploy a service
async fn deploy_service(
    State(state): State<SharedState>,
    Path(name): Path<String>,
) -> Result<Json<String>, ServiceManagerError> {
    println!("Deploying service");
    let mut service_manager = state.lock().await;
    service_manager.deploy_service(&name).await?;
    Ok(Json("Service deployed".to_string()))
}

/// Start a service
async fn start_service(
    State(state): State<SharedState>,
    Path(name): Path<String>,
) -> Result<Json<String>, ServiceManagerError> {
    println!("Starting service");
    let mut service_manager = state.lock().await;
    service_manager.start_service(&name).await?;
    Ok(Json("Service started".to_string()))
}

/// Stop a service
async fn stop_service(
    State(state): State<SharedState>,
    Path(name): Path<String>,
) -> Result<Json<String>, ServiceManagerError> {
    println!("Stopping service");
    let mut service_manager = state.lock().await;
    service_manager.stop_service(&name).await?;
    Ok(Json("Service stopped".to_string()))
}