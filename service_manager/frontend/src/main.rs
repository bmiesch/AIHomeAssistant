mod parser;

use leptos::*;
use wasm_bindgen::prelude::*;
use web_sys::WebSocket;
use serde::{Deserialize, Serialize};
use gloo_console as console;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Service {
    pub name: String,
    pub device: String,
    pub status: String,
}

#[derive(Clone, Debug, Serialize)]
pub struct CreateServiceRequest {
    pub name: String,
    pub device_name: String,
}

// API client functions
mod api {
    use super::*;
    
    const API_BASE: &str = "http://localhost:3000";

    pub async fn fetch_services() -> Result<Vec<Service>, reqwest::Error> {
        reqwest::Client::new()
            .get(&format!("{}/services", API_BASE))
            .header("Accept", "application/json")
            .send()
            .await?
            .json()
            .await
    }

    pub async fn deploy_service(name: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/services/{}/deploy", API_BASE, name))
            .header("Accept", "application/json")
            .send()
            .await?;
        Ok(())
    }

    pub async fn start_service(name: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/services/{}/start", API_BASE, name))
            .header("Accept", "application/json")
            .send()
            .await?;
        Ok(())
    }

    pub async fn stop_service(name: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/services/{}/stop", API_BASE, name))
            .header("Accept", "application/json")
            .send()
            .await?;
        Ok(())
    }

    pub async fn create_service(name: &str, device: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/services/{}", API_BASE, name))
            .json(&CreateServiceRequest {
                name: name.to_string(),
                device_name: device.to_string(),
            })
            .send()
            .await?;
        Ok(())
    }

    pub async fn remove_service(name: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .delete(&format!("{}/services/{}/remove", API_BASE, name))
            .header("Accept", "application/json")
            .send()
            .await?;
        Ok(())
    }
}

#[component]
fn WebSocketComponent() -> impl IntoView {
    let (messages, set_messages) = create_signal(String::new());

    let clear_messages = move |_| set_messages.set(String::new());

    spawn_local(async move {
        let ws = WebSocket::new("ws://localhost:9001")
            .expect("Failed to create WebSocket connection");

        let onmessage_callback = Closure::wrap(Box::new(move |e: web_sys::MessageEvent| {
            if let Some(data) = e.data().as_string() {
                // Debug print raw message
                console::log!("Raw message:", &data);
                
                let formatted_message = parser::format_message(&data);
                
                // Debug print formatted message
                console::log!("Formatted:", &formatted_message);
                set_messages.update(|msg| msg.push_str(&format!("{}\n", formatted_message)));
            }
        }) as Box<dyn FnMut(_)>);

        let onerror_callback = Closure::wrap(Box::new(move |e: web_sys::ErrorEvent| {
            println!("WebSocket error: {:?}", e);
        }) as Box<dyn FnMut(_)>);

        ws.set_onmessage(Some(onmessage_callback.as_ref().unchecked_ref()));
        ws.set_onerror(Some(onerror_callback.as_ref().unchecked_ref()));

        onmessage_callback.forget();
        onerror_callback.forget();
    });

    view! {
        <div class="bg-white rounded-lg shadow p-4 mt-4">
            <div class="flex justify-between items-center mb-4">
                <h2 class="text-lg font-semibold">"Service Logs"</h2>
                <button
                    class="bg-gray-500 hover:bg-gray-600 text-white px-3 py-1 rounded text-sm"
                    on:click=clear_messages
                >
                    "Clear"
                </button>
            </div>
            <div class="h-64 overflow-y-auto">
                <pre class="whitespace-pre-wrap break-words text-sm text-gray-600 font-mono">
                    {move || messages.get()}
                </pre>
            </div>
        </div>
    }
}

#[component]
fn ServiceForm(on_submit: Action<(), Result<(), reqwest::Error>>) -> impl IntoView {
    let (name, set_name) = create_signal(String::new());
    let (device, set_device) = create_signal(String::new());

    let create_service = create_action(move |_| {
        let name = name.get();
        let device = device.get();
        async move {
            api::create_service(&name, &device).await?;
            on_submit.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    view! {
        <form
            class="bg-white rounded-lg shadow p-4 ml-4 max-w-md mx-auto"
            on:submit=move |ev| {
                ev.prevent_default();
                create_service.dispatch(());
                // Clear the form
                set_name.update(|s| *s = String::new());
                set_device.update(|s| *s = String::new());
            }
        >
            <h3 class="text-lg font-semibold mb-4">"Add New Service"</h3>
            <div class="space-y-4">
                <div>
                    <label class="block text-sm font-medium text-gray-700">"Service Name"</label>
                    <input
                        type="text"
                        class="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
                        required
                        prop:value=move || name.get()
                        on:input=move |ev| {
                            set_name.set(event_target_value(&ev));
                        }
                    />
                </div>
                <div>
                    <label class="block text-sm font-medium text-gray-700">"Device"</label>
                    <input
                        type="text"
                        class="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
                        required
                        prop:value=move || device.get()
                        on:input=move |ev| {
                            set_device.set(event_target_value(&ev));
                        }
                    />
                </div>
                <button
                    type="submit"
                    class="w-full bg-blue-500 hover:bg-blue-600 text-white px-4 py-2 rounded"
                >
                    "Add Service"
                </button>
            </div>
        </form>
    }
}

#[component]
fn ServiceCard(
    service: Service,
    on_status_change: Action<(), Result<(), reqwest::Error>>,
) -> impl IntoView {

    let service = create_rw_signal(service);

    let status_class = move || match service.get().status.as_str() {
        "Running" => "bg-green-100 text-green-800",
        "Stopped" => "bg-red-100 text-red-800",
        _ => "bg-gray-100 text-gray-800",
    };

    let deploy = create_action(move |_| {
        let name = service.get().name.clone();
        async move { api::deploy_service(&name).await }
    });

    let start = create_action(move |_| {
        let name = service.get().name.clone();
        async move { api::start_service(&name).await }
    });

    let stop = create_action(move |_| {
        let name = service.get().name.clone();
        async move { api::stop_service(&name).await }
    });

    let remove = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::remove_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    create_effect(move |_| {
        if start.version().get() > 0 || stop.version().get() > 0 {
            on_status_change.dispatch(());
        }
    });

    view! {
        <div class="bg-white rounded-lg shadow p-4">
            <h3 class="text-lg font-semibold mb-2">{move || service.get().name.clone()}</h3>
            <div class="flex justify-between items-center mb-4">
                <span class={move || format!("px-2 py-1 rounded-full {}", status_class())}>
                    {move || service.get().status.clone()}
                </span>
                <span class="text-gray-600">
                    {move || service.get().device.clone()}
                </span>
            </div>
            <div class="flex space-x-2">
                <button
                    class="bg-gray-500 hover:bg-gray-600 text-white px-4 py-2 rounded
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-gray-500"
                    on:click=move |_| deploy.dispatch(())
                    disabled=move || service.get().status == "Deployed"
                >
                    "Deploy"
                </button>
                <button
                    class="bg-blue-500 hover:bg-blue-600 text-white px-4 py-2 rounded
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-blue-500"
                    on:click=move |_| start.dispatch(())
                    disabled=move || service.get().status == "Running"
                >
                    "Start"
                </button>
                <button
                    class="bg-red-500 hover:bg-red-600 text-white px-4 py-2 rounded
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-red-500"
                    on:click=move |_| stop.dispatch(())
                    disabled=move || service.get().status == "Stopped" || service.get().status != "Running"
                >   
                    "Stop"
                </button>
                <button
                    class="bg-gray-500 hover:bg-gray-600 text-white px-4 py-2 rounded
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-gray-500"
                    on:click=move |_| remove.dispatch(())
                >
                    "Remove"
                </button>
            </div>
        </div>
    }
}

#[component]
fn App() -> impl IntoView {
    let (services, set_services) = create_signal(Vec::new());
    let (error, set_error) = create_signal(None::<String>);

    let refresh = create_action(move |_| async move {
        match api::fetch_services().await {
            Ok(new_services) => {
                set_services.update(|s| *s = new_services);
                Ok(())
            }
            Err(e) => {
                set_error.update(|err| *err = Some(e.to_string()));
                Err(e)
            }
        }
    });

    // Initial load of services
    create_effect(move |_| {
        refresh.dispatch(());
    });

    view! {
        <div class="flex">
            <div class="w-1/3 p-4">
                <h1 class="text-2xl font-bold mb-4">"Service Manager"</h1>

                // Error messages
                {move || error.get().map(|err| view! {
                    <div class="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded mb-4">
                        {err}
                    </div>
                })}

                <div class="bg-white rounded-lg shadow p-4">
                    <h2 class="text-lg font-semibold mb-4">"Services"</h2>
                    
                    <ServiceForm
                        on_submit=refresh.clone()
                    />

                    <div class="space-y-4 mt-4">
                        {move || services.get().into_iter().map(|service| {
                            view! {
                                <ServiceCard
                                    service=service
                                    on_status_change=refresh.clone()
                                />
                            }
                        }).collect::<Vec<_>>()}
                    </div>
                </div>
            </div>

            <div class="w-1/3 p-4">
                <WebSocketComponent />
            </div>
        </div>
    }
}

fn main() {
    mount_to_body(App)
}