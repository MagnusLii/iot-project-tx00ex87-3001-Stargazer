use crate::{sg_api, sg_auth};
use axum::{
    body::{to_bytes, Body},
    http::StatusCode,
    response::{Html, IntoResponse},
    routing::{delete, get, post},
    Router,
};
use axum_login::{
    login_required,
    tower_sessions::{MemoryStore, SessionManagerLayer},
    AuthManagerLayerBuilder,
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use sqlx::SqlitePool;
use std::{env, fs};
use tower_http::services::ServeDir;
use webserver::ImageDirectory;

pub fn configure(user_db: SqlitePool) -> Router<sg_api::ApiState> {
    let session_store = MemoryStore::default();
    let session_layer = SessionManagerLayer::new(session_store);
    let backend = sg_auth::Backend::new(user_db);
    let auth_layer = AuthManagerLayerBuilder::new(backend, session_layer).build();

    Router::<_>::new()
        .route("/", get(root))
        .route("/gallery", get(gallery))
        .route("/control", get(control))
        //.route("/control/command", post(command))
        .route("/control/keys", get(sg_api::api_keys))
        .route("/control/keys", post(sg_api::new_key))
        .route("/control/keys", delete(sg_api::delete_key))
        .nest_service("/images", ServeDir::new("images"))
        .route_layer(login_required!(sg_auth::Backend, login_url = "/login"))
        .route("/login", get(login_page))
        .route("/login", post(sg_auth::login))
        .layer(auth_layer)
        .nest_service("/assets", ServeDir::new("assets"))
        .route("/api/upload", post(upload))
        //.route("/api/command", get(command))
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

// TODO: Maybe look in to using proper html templates
pub async fn update_gallery() -> bool {
    let tmp = env::temp_dir();
    let directory = ImageDirectory::default();
    let images = directory.find_images();

    let mut html = std::include_str!("../html/images.html").to_string();
    let html_images = images
        .iter()
        .map(|image| format!("<img src=\"{}\"/>", image.display().to_string()))
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--IMAGES-->", &html_images);

    println!(
        "Write path: {}",
        &tmp.join("sgwebserver/images.html").display()
    );

    let result = match fs::write(&tmp.join("sgwebserver/images.html"), html) {
        Ok(_) => true,
        Err(e) => {
            eprintln!("Error updating images: {}", e);
            false
        }
    };

    result
}

async fn gallery() -> impl IntoResponse {
    let tmp = env::temp_dir();

    if let Ok(cached_html) = fs::read(&tmp.join("sgwebserver/images.html")) {
        return (StatusCode::OK, Html(cached_html));
    }

    let updated = update_gallery().await;

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

async fn control() -> impl IntoResponse {
    (
        StatusCode::OK,
        Html(std::include_str!("../html/control.html")),
    )
}

// Parse the body of the POST request for base64 encoded image
// TODO: Handle errors
async fn upload(body: Body) -> impl IntoResponse {
    let bytes = to_bytes(body, 262144000).await.unwrap();

    let converted = String::from_utf8(bytes.to_vec()).unwrap();
    //println!("{}", converted);

    let decoded = general_purpose::STANDARD.decode(converted).unwrap();

    let now = Utc::now().timestamp();
    //println!("{}", now);

    // Write path, TODO: Can technically be a collision
    let path = format!("./images/{}.png", now);

    fs::write(path, decoded).unwrap();

    update_gallery().await;

    (StatusCode::OK, "Success\n")
}

async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../html/404.html")),
    )
}
