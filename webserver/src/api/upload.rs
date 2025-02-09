use crate::{api::commands, err::Error, keys::verify_key, web::images, SharedState};
use axum::{
    extract::{rejection::JsonRejection, Json, State},
    http::StatusCode,
    response::IntoResponse,
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use infer;
use serde::Deserialize;
use tokio::fs;

use sqlx::{FromRow, SqlitePool};

#[derive(Deserialize)]
pub struct UploadImage {
    token: String,
    data: String,
    id: i64,
}

// Custom debug format for UploadImage
impl std::fmt::Debug for UploadImage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("UploadImage")
            .field("token", &self.token)
            .field("id", &self.id)
            .field("data length", &self.data.len())
            .finish()
    }
}

// Parse the body of the POST request for base64 encoded image
pub async fn upload_image(
    State(state): State<SharedState>,
    payload: Result<Json<UploadImage>, JsonRejection>,
) -> impl IntoResponse {
    println!("Received upload request");
    let contents = match payload {
        Ok(img_payload) => {
            dbg!(&img_payload);
            img_payload
        }
        Err(img_payload) => {
            dbg!("Error parsing JSON payload", img_payload);
            return (StatusCode::BAD_REQUEST, "Error parsing payload\n");
        }
    };

    if !verify_key(&contents.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    let data = &contents.data;

    let decoded = match general_purpose::STANDARD.decode(data) {
        Ok(decoded_data) => decoded_data,
        Err(e) => {
            eprintln!("Error decoding image: {}", e);
            return (StatusCode::BAD_REQUEST, "Unable to decode image\n");
        }
    };

    if !infer::is_image(&decoded) {
        eprintln!("Error with image: data received didn't contain an image");
        return (
            StatusCode::BAD_REQUEST,
            "Data sent doesn't appear to be an image\n",
        );
    }

    let file_info = if let Some(info) = infer::get(&decoded) {
        info
    } else {
        // Probably shouldn't ever happen since we checked with infer::is_image above
        eprintln!("Error with image: could not get file info");
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    let now = Utc::now().timestamp();

    let name = if let Ok(name) = get_image_name(&state.db, contents.id).await {
        name
    } else {
        ImageName {
            target: "unknown".to_string(),
            position: "unknown".to_string(),
        }
    };

    let filename = format!("{}-{}-{}.{}", now, contents.id, name, file_info.extension());
    let web_path = format!("/assets/images/{}", filename);
    let pathbuf = state.image_dir.path.join(filename);
    let path = if let Some(path) = pathbuf.to_str() {
        path
    } else {
        eprintln!("Error writing image to disk: could not get path");
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    if let Err(e) = fs::write(&pathbuf, decoded).await {
        eprintln!("Error writing image to disk: {}", e);
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    if let Err(e) =
        images::register_image(&state.db, &name.to_string(), &path, &web_path, contents.id).await
    {
        eprintln!("Error registering image: {}", e);
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    commands::modify_command_status(&state.db, contents.id, 3).await; // Mark as uploaded (status = 3)

    println!("Uploaded image: {}", path);

    (StatusCode::OK, "Success\n")
}

#[derive(Debug, Deserialize, FromRow)]
pub struct ImageName {
    pub target: String,
    pub position: String,
}

impl std::fmt::Display for ImageName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}_{}", self.target, self.position)
    }
}

pub async fn get_image_name(db: &SqlitePool, id: i64) -> Result<ImageName, Error> {
    let command = sqlx::query_as(
        "SELECT objects.name AS target, object_positions.position AS position 
        FROM commands
        JOIN objects ON commands.target = objects.id
        JOIN object_positions ON commands.position = object_positions.id
        WHERE commands.id = ?",
    )
    .bind(id)
    .fetch_one(db)
    .await?;

    Ok(command)
}
