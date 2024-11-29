use axum::{
    body::{to_bytes, Body},
    http::StatusCode,
    response::{Html, IntoResponse},
    routing::{get, post},
    Router,
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use std::{env, fs};
use tokio::net::TcpListener;
use tower_http::services::ServeDir;
use webserver::ImageDirectory;

#[tokio::main]
async fn main() {
    let address = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:7878".to_string());

    let app: Router = Router::new()
        .route("/", get(root))
        .route("/images", get(images))
        .route("/upload", post(upload))
        .nest_service("/assets", ServeDir::new("assets"))
        .fallback(unknown_route);

    let listener = TcpListener::bind(&address).await.unwrap();

    println!("Listening on: {}", address);

    axum::serve(listener, app).await.unwrap();
}

async fn root() -> impl IntoResponse {
    (StatusCode::OK, Html(std::include_str!("../index.html")))
}

async fn images() -> impl IntoResponse {
    let directory = ImageDirectory::default();
    let images = directory.find_images();

    println!("{:?}", images);

    let html = images
        .iter()
        .map(|image| format!("<img src=\"{}\"/>", image.display().to_string()))
        .collect::<Vec<String>>()
        .join("\n");

    (StatusCode::OK, Html(html))
}

// Parse the body of the POST request for base64 encoded image
// TODO: Handle errors
async fn upload(body: Body) -> impl IntoResponse {
    let bytes = to_bytes(body, 262144000).await.unwrap();

    let converted = String::from_utf8(bytes.to_vec()).unwrap();
    println!("{}", converted);

    let decoded = general_purpose::STANDARD.decode(converted).unwrap();

    let now = Utc::now().timestamp();
    println!("{}", now);

    // Write path, TODO: Can technically be a collision
    let path = format!("./assets/{}.png", now);

    fs::write(path, decoded).unwrap();

    (StatusCode::OK, "Success\n")
}

async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../404.html")),
    )
}
