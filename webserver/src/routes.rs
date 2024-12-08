use crate::sg::{api, auth, images::ImageDirectory};
use axum::{
    extract::{DefaultBodyLimit, State},
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
use sqlx::SqlitePool;
use std::{env, fs};
use tower_http::services::ServeDir;

pub fn configure(user_db: SqlitePool) -> Router<api::ApiState> {
    let session_store = MemoryStore::default();
    let session_layer = SessionManagerLayer::new(session_store);
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

async fn control(State(state): State<api::ApiState>) -> impl IntoResponse {
    let mut html = std::include_str!("../html/control.html").to_string();
    let html_keys = api::get_api_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| format!("<option>{}</option>", key.id))
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    (StatusCode::OK, Html(html))
}

/*
#[derive(Deserialize)]
pub struct UploadImage {
    key: String,
    data: String,
}

// Parse the body of the POST request for base64 encoded image
// TODO: Handle errors
async fn upload(
    State(state): State<api::ApiState>,
    Json(payload): Json<UploadImage>,
) -> impl IntoResponse {
    //let bytes = to_bytes(payload.data, 262144000).await.unwrap();
    if !api::verify_key(&payload.key, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    let data = payload.data;
    println!("{}", data);

    let decoded = general_purpose::STANDARD.decode(data).unwrap();
    assert!(infer::is_image(&decoded));

    let file_info = infer::get(&decoded).expect("file type is known");
    println!("{:?}", file_info.mime_type());

    let now = Utc::now().timestamp();
    //println!("{}", now);

    // Write path, TODO: Can technically be a collision
    let path = format!("./images/{}.{}", now, file_info.extension());

    fs::write(path, decoded).unwrap();

    update_gallery().await;

    (StatusCode::OK, "Success\n")
}
*/
async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../html/404.html")),
    )
}
