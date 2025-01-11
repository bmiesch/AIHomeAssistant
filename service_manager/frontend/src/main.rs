mod parser;

use leptos::*;
use wasm_bindgen::prelude::*;
use web_sys::WebSocket;
use serde::{Deserialize, Serialize};
use gloo_console as console;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Service {
    pub name: String,
    pub status: String,
    pub enabled: bool,
    pub device: String,
}

#[derive(Clone, Debug, Serialize)]
pub struct CreateServiceRequest {
    pub name: String,
    pub device_name: String,
}

#[derive(Clone, Debug, Serialize)]
pub struct PublishMessageRequest {
    pub topic: String,
    pub payload: String,
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

    pub async fn publish_mqtt(topic: &str, payload: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/mqtt/publish", API_BASE))
            .json(&PublishMessageRequest {
                topic: topic.to_string(),
                payload: payload.to_string(),
            })
            .send()
            .await?;
        Ok(())
    }

    pub async fn enable_service(name: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/services/{}/enable", API_BASE, name))
            .header("Accept", "application/json")
            .send()
            .await?;
        Ok(())
    }

    pub async fn disable_service(name: &str) -> Result<(), reqwest::Error> {
        reqwest::Client::new()
            .post(&format!("{}/services/{}/disable", API_BASE, name))
            .header("Accept", "application/json")
            .send()
            .await?;
        Ok(())
    }
}

#[derive(Clone, Debug)]
struct KeyValuePair {
    key: RwSignal<String>,
    value: RwSignal<String>,
}

#[component]
fn MqttPublisher() -> impl IntoView {
    let (topic, set_topic) = create_signal(String::new());
    let (pairs, set_pairs) = create_signal(vec![KeyValuePair { 
        key: create_rw_signal(String::new()),
        value: create_rw_signal(String::new()),
    }]);
    let (status, set_status) = create_signal(Option::<String>::None);

    let add_pair = move |_| {
        set_pairs.update(|pairs| {
            pairs.push(KeyValuePair { 
                key: create_rw_signal(String::new()),
                value: create_rw_signal(String::new()),
            });
        });
    };

    let remove_pair = move |index: usize| {
        set_pairs.update(|pairs| {
            pairs.remove(index);
            if pairs.is_empty() {
                pairs.push(KeyValuePair { 
                    key: create_rw_signal(String::new()),
                    value: create_rw_signal(String::new()),
                });
            }
        });
    };

    let publish = create_action(move |_| {
        let topic = topic.get();
        let payload = {
            let mut map = serde_json::Map::new();
            for pair in pairs.get().iter() {
                if !pair.key.get().is_empty() {
                    map.insert(
                        pair.key.get(),
                        serde_json::Value::String(pair.value.get())
                    );
                }
            }
            serde_json::Value::Object(map).to_string()
        };

        async move {
            match api::publish_mqtt(&topic, &payload).await {
                Ok(_) => {
                    set_status.set(Some("Message published successfully".to_string()));
                    set_timeout(
                        move || set_status.set(None),
                        std::time::Duration::from_secs(3)
                    );
                    Ok(())
                },
                Err(e) => {
                    set_status.set(Some(format!("Error: {}", e)));
                    Err(e)
                }
            }
        }
    });

    view! {
        <div class="bg-white rounded-lg shadow p-4">
            <h2 class="text-lg font-semibold mb-4">"MQTT Publisher"</h2>
            
            <form
                class="space-y-4"
                on:submit=move |ev| {
                    ev.prevent_default();
                    publish.dispatch(());
                }
            >
                <div>
                    <label class="block text-sm font-medium text-gray-700">"Topic"</label>
                    <input
                        type="text"
                        class="mt-1 block w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
                        required
                        placeholder="home/services/led_manager/command"
                        prop:value=move || topic.get()
                        on:input=move |ev| {
                            set_topic.set(event_target_value(&ev));
                        }
                    />
                    <p class="mt-1 text-sm text-gray-500">
                        "Example topics: home/services/{service}/command, home/services/{service}/status"
                    </p>
                </div>

                <div class="space-y-2">
                    <label class="block text-sm font-medium text-gray-700">"Message Content"</label>
                    
                    <div class="space-y-2">
                        {move || pairs.get().into_iter().enumerate().map(|(index, pair)| {
                            view! {
                                <div class="flex gap-2 items-start">
                                    <div class="flex-1">
                                        <input
                                            type="text"
                                            class="w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
                                            placeholder="Key"
                                            prop:value=move || pair.key.get()
                                            on:input=move |ev| {
                                                pair.key.set(event_target_value(&ev));
                                            }
                                        />
                                    </div>
                                    <div class="flex-1">
                                        <input
                                            type="text"
                                            class="w-full rounded-md border-gray-300 shadow-sm focus:border-blue-500 focus:ring-blue-500"
                                            placeholder="Value"
                                            prop:value=move || pair.value.get()
                                            on:input=move |ev| {
                                                pair.value.set(event_target_value(&ev));
                                            }
                                        />
                                    </div>
                                    <button
                                        type="button"
                                        class="px-2 py-1 text-sm text-red-600 hover:text-red-800"
                                        on:click=move |_| remove_pair(index)
                                        disabled=move || pairs.get().len() == 1
                                    >
                                        "✕"
                                    </button>
                                </div>
                            }
                        }).collect_view()}
                    </div>

                    <button
                        type="button"
                        class="mt-2 text-sm text-blue-600 hover:text-blue-800"
                        on:click=add_pair
                    >
                        "+ Add Field"
                    </button>
                </div>

                <div>
                    <label class="block text-sm font-medium text-gray-700">"JSON Preview"</label>
                    <pre class="mt-1 p-2 bg-gray-50 rounded-md text-sm font-mono overflow-x-auto">
                        {move || {
                            let mut map = serde_json::Map::new();
                            for pair in pairs.get().iter() {
                                if !pair.key.get().is_empty() {
                                    map.insert(
                                        pair.key.get(),
                                        serde_json::Value::String(pair.value.get())
                                    );
                                }
                            }
                            serde_json::to_string_pretty(&serde_json::Value::Object(map))
                                .unwrap_or_else(|_| "{}".to_string())
                        }}
                    </pre>
                </div>

                {move || status.get().map(|msg| {
                    let msg_for_class = msg.clone();
                    let msg_for_content = msg.clone();
                    view! {
                        <div class=move || {
                            let base_classes = "mt-2 p-2 rounded text-sm";
                            if msg_for_class.as_str().starts_with("Error") {
                                format!("{} bg-red-100 text-red-700", base_classes)
                            } else {
                                format!("{} bg-green-100 text-green-700", base_classes)
                            }
                        }>
                            {msg_for_content}
                        </div>
                    }
                })}

                <button
                    type="submit"
                    class="w-full bg-blue-500 hover:bg-blue-600 text-white px-4 py-2 rounded"
                    disabled=move || publish.pending().get()
                >
                    {move || if publish.pending().get() { "Publishing..." } else { "Publish Message" }}
                </button>
            </form>
        </div>
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

    let enabled_class = move || if service.get().enabled {
        "bg-green-100 text-green-800"
    } else {
        "bg-red-100 text-red-800"
    };

    let deploy = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::deploy_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    let start = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::start_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    let stop = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::stop_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    let remove = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::remove_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    let enable = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::enable_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    let disable = create_action(move |_| {
        let name = service.get().name.clone();
        async move {
            api::disable_service(&name).await?;
            on_status_change.dispatch(());
            Ok::<(), reqwest::Error>(())
        }
    });

    view! {
        <div class="bg-white rounded-lg shadow p-4">
            <h3 class="text-lg font-semibold mb-2">{move || service.get().name.clone()}</h3>
            <div class="flex justify-between items-center mb-4">
                <div class="flex space-x-2">
                    <span class={move || format!("px-2 py-1 rounded-full {}", status_class())}>
                        {move || service.get().status.clone()}
                    </span>
                    <span class={move || format!("px-2 py-1 rounded-full {}", enabled_class())}>
                        {move || service.get().enabled.to_string()}
                    </span>
                </div>
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
                <button
                    class="bg-gray-500 hover:bg-gray-600 text-white px-4 py-2 rounded
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-gray-500"
                    on:click=move |_| enable.dispatch(())
                >
                    "Enable"
                </button>
                <button
                    class="bg-gray-500 hover:bg-gray-600 text-white px-4 py-2 rounded
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-gray-500"
                    on:click=move |_| disable.dispatch(())
                >
                    "Disable"
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
                set_error.set(None);
                set_services.update(|s| *s = new_services);
                Ok(())
            }
            Err(e) => {
                // If status is None, it usually means we couldn't connect to the server
                let error_msg = if e.status().is_none() {
                    "Backend service is unavailable. Please check if the server is running.".to_string()
                } else {
                    e.to_string()
                };
                set_error.set(Some(error_msg));
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

            <div class="w-1/3 p-4">
                <div class="mt-4">
                    <MqttPublisher />
                </div>
            </div>
        </div>
    }
}

fn main() {
    mount_to_body(App)
}