use crate::{api::ApiState, keys::verify_key, web::images};
use axum::{
    extract::{Json, State},
    http::StatusCode,
    response::IntoResponse,
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use infer;
use serde::Deserialize;
use std::fs;

#[derive(Deserialize)]
pub struct UploadImage {
    token: String,
    data: String,
}

// Parse the body of the POST request for base64 encoded image
// TODO: Handle errors
pub async fn upload_image(
    State(state): State<ApiState>,
    Json(payload): Json<UploadImage>,
) -> impl IntoResponse {
    if !verify_key(&payload.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    let data = payload.data;

    let decoded = general_purpose::STANDARD.decode(data).unwrap();
    assert!(infer::is_image(&decoded));

    let file_info = infer::get(&decoded).expect("file type is known");
    println!("{:?}", file_info.mime_type());

    let now = Utc::now().timestamp();

    // Write path, TODO: Can technically be a collision
    let path = format!("assets/images/{}.{}", now, file_info.extension());

    fs::write(path, decoded).unwrap();

    images::update_gallery().await;

    (StatusCode::OK, "Success\n")
}
