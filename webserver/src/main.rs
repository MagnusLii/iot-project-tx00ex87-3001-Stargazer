use axum::{
    body::{to_bytes, Body},
    http::StatusCode,
    response::{Html, IntoResponse},
    routing::{get, post},
    Router,
};
use axum_login::{
    login_required,
    tower_sessions::{MemoryStore, SessionManagerLayer},
    AuthManagerLayerBuilder,
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use sqlx::{sqlite::SqliteConnectOptions, SqlitePool};
use std::{env, fs};
use tokio::net::TcpListener;
use tower_http::services::ServeDir;
use webserver::ImageDirectory;

mod sg_auth;

#[tokio::main]
async fn main() {
    let address = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:7878".to_string());

    // Does database exist?
    let first_run = !std::path::Path::new("db/users.db").exists();

    let options = SqliteConnectOptions::new()
        .filename("db/users.db")
        .create_if_missing(true);
    let db = SqlitePool::connect_with(options).await.unwrap();
    // Create tables and admin user if first run
    if first_run {
        println!("First run detected. Creating tables and admin user");
        sg_auth::create_user_table(&db).await;
        sg_auth::create_api_keys_table(&db).await;
        sg_auth::create_admin(&db).await;
    }

    let session_store = MemoryStore::default();
    let session_layer = SessionManagerLayer::new(session_store);
    let backend = sg_auth::Backend::new(db);
    let auth_layer = AuthManagerLayerBuilder::new(backend, session_layer).build();

    // Create tmp dir for caching
    let tmp = env::temp_dir();
    match fs::create_dir_all(&tmp.join("sgwebserver")) {
        Ok(_) => println!("Created tmp dir"),
        Err(e) => panic!("Error creating tmp dir: {}", e),
    };

    // Update images
    update_images().await;

    let app: Router = Router::new()
        .route("/", get(root))
        .route("/images", get(images))
        .nest_service("/assets", ServeDir::new("assets"))
        .route_layer(login_required!(sg_auth::Backend, login_url = "/login"))
        .route("/login", get(login_page))
        .route("/login", post(sg_auth::login))
        .layer(auth_layer)
        .route("/upload", post(upload))
        .fallback(unknown_route);

    let listener = TcpListener::bind(&address).await.unwrap();

    println!("Listening on: {}", address);

    axum::serve(listener, app).await.unwrap();
}

async fn root() -> impl IntoResponse {
    (StatusCode::OK, Html(std::include_str!("../index.html")))
}

async fn login_page() -> impl IntoResponse {
    (StatusCode::OK, Html(std::include_str!("../login.html")))
}

// TODO: Maybe look in to using proper html templates
async fn update_images() -> bool {
    let tmp = env::temp_dir();
    let directory = ImageDirectory::default();
    let images = directory.find_images();

    let mut html = std::include_str!("../images.html").to_string();
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

async fn images() -> impl IntoResponse {
    let tmp = env::temp_dir();

    if let Ok(cached_html) = fs::read(&tmp.join("sgwebserver/images.html")) {
        println!("Using cached images.html");
        return (StatusCode::OK, Html(cached_html));
    }

    let updated = update_images().await;

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

    update_images().await;

    (StatusCode::OK, "Success\n")
}

async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../404.html")),
    )
}
