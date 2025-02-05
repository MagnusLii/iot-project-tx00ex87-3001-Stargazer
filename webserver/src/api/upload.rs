use crate::{api::commands, keys::verify_key, web::images, SharedState};
use axum::{
    body::Body,
    extract::{rejection::JsonRejection, Json, RawForm, Request, State},
    http::StatusCode,
    response::IntoResponse,
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use infer;
use serde::Deserialize;
use tokio::fs;

#[derive(Debug, Deserialize)]
pub struct UploadImage {
    token: String,
    data: String,
    id: i64,
}

// Parse the body of the POST request for base64 encoded image
pub async fn upload_image(
    State(state): State<SharedState>,
    payload: Result<Json<UploadImage>, JsonRejection>,
) -> impl IntoResponse {
    println!("Received upload request");
    let contents;
    match payload {
        Ok(payload) => {
            println!("Parsed payload: {:#?}", payload);
            contents = payload;
        }
        Err(payload) => {
            dbg!(payload);
            return (StatusCode::BAD_REQUEST, "Error parsing payload\n");
        }
    };

    if !verify_key(&contents.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }
    println!("Verified key");

    let data = &contents.data;

    let decoded = general_purpose::STANDARD.decode(data).unwrap();
    assert!(infer::is_image(&decoded));

    let file_info = infer::get(&decoded).expect("file type is known");
    println!("{:?}", file_info.mime_type());

    let now = Utc::now().timestamp();

    let name = "TEMPNAME";

    let filename = format!("{}-{}-{}.{}", now, contents.id, name, file_info.extension());
    let web_path = format!("/assets/images/{}", filename);
    let pathbuf = state.image_dir.path.join(filename);
    let path = pathbuf.to_str().unwrap();

    if let Err(e) = fs::write(&pathbuf, decoded).await {
        eprintln!("Error writing image to disk: {}", e); // TODO: Do something
    };

    if let Err(e) = images::register_image(&state.db, &name, &path, &web_path, contents.id).await {
        eprintln!("Error registering image: {}", e); // TODO: Do something
    };

    commands::modify_command_status(&state.db, contents.id, 3).await; // Mark as uploaded (status = 3)

    (StatusCode::OK, "Success\n")
}

pub async fn print_request(
    State(_state): State<SharedState>,
    request: Request<Body>,
) -> impl IntoResponse {
    dbg!(&request);
    (StatusCode::OK, "Success\n")
}
