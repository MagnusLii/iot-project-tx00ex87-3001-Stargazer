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

/// Represents an image upload request payload.
#[derive(Deserialize)]
pub struct UploadImage {
    /// Authentication token for authorization.
    token: String,
    /// Base64-encoded image data.
    data: String,
    /// Unique identifier for the image.
    id: i64,
}

/// Custom debug format for UploadImage
impl std::fmt::Debug for UploadImage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("UploadImage")
            .field("token", &self.token)
            .field("id", &self.id)
            .field("data length", &self.data.len())
            .finish()
    }
}

/// Handles image upload requests.
/// 
/// This function processes a POST request containing a base64-encoded image,
/// verifies authentication, decodes and validates the image, saves it to disk,
/// and registers the image in the database.
/// 
/// # Arguments
/// 
/// * `state` - Shared application state.
/// * `payload` - The JSON request payload containing the image and authentication token.
/// 
/// # Returns
/// 
/// A HTTP response indicating success or failure.
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

    // Verify API token
    if !verify_key(&contents.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    let data = &contents.data;

    // Decode the base64 image data
    let decoded = match general_purpose::STANDARD.decode(data) {
        Ok(decoded_data) => decoded_data,
        Err(e) => {
            eprintln!("Error decoding image: {}", e);
            return (StatusCode::BAD_REQUEST, "Unable to decode image\n");
        }
    };

    // Validate that the decoded data is an image
    if !infer::is_image(&decoded) {
        eprintln!("Error with image: data received didn't contain an image");
        return (
            StatusCode::BAD_REQUEST,
            "Data sent doesn't appear to be an image\n",
        );
    }

    // Retrieve file type information
    let file_info = if let Some(info) = infer::get(&decoded) {
        info
    } else {
        // Probably shouldn't ever happen since we checked with infer::is_image above
        eprintln!("Error with image: could not get file info");
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    let now = Utc::now().timestamp();

    // Retrieve image name from database
    let name = if let Ok(name) = get_image_name(&state.db, contents.id).await {
        name
    } else {
        ImageName {
            target: "unknown".to_string(),
            position: "unknown".to_string(),
        }
    };

    // Construct file name and paths
    let filename = format!("{}-{}-{}.{}", now, contents.id, name, file_info.extension());
    let web_path = format!("/assets/images/{}", filename);
    let pathbuf = state.image_dir.path.join(filename);
    let path = if let Some(path) = pathbuf.to_str() {
        path
    } else {
        eprintln!("Error writing image to disk: could not get path");
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    // Save image to disk
    if let Err(e) = fs::write(&pathbuf, decoded).await {
        eprintln!("Error writing image to disk: {}", e);
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    // Register image in database
    if let Err(e) =
        images::register_image(&state.db, &name.to_string(), &path, &web_path, contents.id).await
    {
        eprintln!("Error registering image: {}", e);
        return (StatusCode::INTERNAL_SERVER_ERROR, "");
    };

    // Update command status to indicate successful upload
    commands::modify_command_status(&state.db, contents.id, 3).await;

    println!("Uploaded image: {}", path);

    (StatusCode::OK, "Success")
}

/// Used to construct an image name based on information retrieved from the database.
#[derive(Debug, Deserialize, FromRow)]
pub struct ImageName {
    /// Target object associated with the image.
    pub target: String,
    /// Position of the object in the image.
    pub position: String,
}

/// Implements Display trait for ImageName to format as "target_position"
impl std::fmt::Display for ImageName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}_{}", self.target, self.position)
    }
}

/// Retrieves an ImageName struct from the database based on its ID.
/// 
/// # Arguments
/// * `db` - Reference to the SQLite connection pool.
/// * `id` - The unique identifier of the image.
/// 
/// # Returns
/// 
/// A `Result` containing an `ImageName` struct or an `Error`.
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
