use crate::sg::{api, auth, images};
use axum::{
    extract::{DefaultBodyLimit, State},
    http::StatusCode,
    response::{Html, IntoResponse},
    routing::{delete, get, post},
    Router,
};
use axum_login::{
    login_required,
    tower_sessions::{cookie::Key, Expiry, MemoryStore, SessionManagerLayer},
    AuthManagerLayerBuilder,
};
use sqlx::SqlitePool;
use std::{env, fs};
use time::Duration;
use tokio::{signal, task::AbortHandle};
use tower_http::services::ServeDir;

pub fn configure(user_db: SqlitePool) -> Router<api::ApiState> {
    let session_store = MemoryStore::default();

    let key = Key::generate();

    let session_layer = SessionManagerLayer::new(session_store)
        .with_secure(false)
        .with_expiry(Expiry::OnInactivity(Duration::days(1)))
        .with_signed(key);

    let backend = auth::Backend::new(user_db);
    let auth_layer = AuthManagerLayerBuilder::new(backend, session_layer).build();

    Router::<_>::new()
        .route("/", get(root))
        .route("/gallery", get(gallery))
        .route("/control", get(control))
        .route("/control/keys", get(api::api_keys))
        .route("/control/keys", post(api::new_key))
        .route("/control/keys", delete(api::delete_key))
        .route("/control/command", post(api::new_command))
        .nest_service("/images", ServeDir::new("images"))
        .route_layer(login_required!(auth::Backend, login_url = "/login"))
        .route("/login", get(login_page))
        .route("/login", post(auth::login))
        .layer(auth_layer)
        .nest_service("/assets", ServeDir::new("assets"))
        .route(
            "/api/upload",
            post(api::upload).layer(DefaultBodyLimit::max(262_144_000)),
        )
        .route("/api/command", get(api::fetch_command))
        .route("/api/diagnostics", post(api::diagnostics))
        .route("/api/time", get(api::time))
        .fallback(unknown_route)
}

async fn root() -> impl IntoResponse {
    (
        StatusCode::OK,
        Html(std::include_str!("../html/index.html")),
    )
}

async fn login_page() -> impl IntoResponse {
    (
        StatusCode::OK,
        Html(std::include_str!("../html/login.html")),
    )
}

async fn gallery() -> impl IntoResponse {
    let tmp = env::temp_dir();

    if let Ok(cached_html) = fs::read(&tmp.join("sgwebserver/images.html")) {
        return (StatusCode::OK, Html(cached_html));
    }

    let updated = images::update_gallery().await;

    let status: StatusCode;
    let html: Vec<u8>;

    if !updated {
        status = StatusCode::INTERNAL_SERVER_ERROR;
        html = "Error updating images".as_bytes().to_vec();
    } else {
        status = StatusCode::OK;
        html = fs::read(&tmp.join("sgwebserver/images.html")).unwrap();
    }

    (status, Html(html.into()))
}

async fn control(State(state): State<api::ApiState>) -> impl IntoResponse {
    let mut html = std::include_str!("../html/control.html").to_string();
    let html_keys = api::get_api_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| format!("<option value=\"{}\">{}</option>", key.id, key.name))
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    let html_commands = api::get_commands(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|command| {
            format!(
                "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                command.id, command.target, command.name, command.associated_key
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_COMMANDS-->", &html_commands);

    (StatusCode::OK, Html(html))
}

async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../html/404.html")),
    )
}
