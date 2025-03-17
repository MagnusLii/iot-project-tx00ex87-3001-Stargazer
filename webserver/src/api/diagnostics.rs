use crate::{keys::verify_key, SharedState};
use axum::{
    extract::{rejection::JsonRejection, Json, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::Deserialize;
use sqlx::SqlitePool;

/// Represents the structure of a diagnostics JSON payload.
#[derive(Deserialize)]
pub struct DiagnosticsJson {
    /// API token for authentication.
    token: String,
    /// Status code representing the type of diagnostic.
    status: i64,
    /// Message containing diagnostic details.
    message: String,
}

/// Handles incoming diagnostics data, verifies the API key, and stores the information in the database.
///
/// # Arguments
/// * `state` - The shared application state containing the database connection pool.
/// * `payload` - The JSON payload containing diagnostics data.
///
/// # Returns
/// A HTTP response indicating success or failure.
pub async fn send_diagnostics(
    State(state): State<SharedState>,
    payload: Result<Json<DiagnosticsJson>, JsonRejection>,
) -> impl IntoResponse {
    let payload = match payload {
        Ok(payload) => payload,
        Err(e) => {
            eprintln!("Error parsing JSON: {}", e);
            return (StatusCode::BAD_REQUEST, "Error parsing JSON");
        }
    };

    // Verify the provided API key
    if !verify_key(&payload.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized");
    }

    // Ensure the status code is within the accepted range
    if payload.status < 1 || payload.status > 3 {
        return (StatusCode::BAD_REQUEST, "Invalid status");
    }

    println!(
        "New diagnostic data: [{}] {}",
        payload.status, payload.message
    );

    // Store the diagnostic information in the database
    if let Err(e) =
        new_diagnostic(&state.db, &payload.token, payload.status, &payload.message).await
    {
        eprintln!("Error saving diagnostic: {}", e);
        return (StatusCode::INTERNAL_SERVER_ERROR, "Error saving diagnostic");
    }

    (StatusCode::OK, "Success")
}

/// Inserts a new diagnostic entry into the database.
///
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// * `token` - The API token associated with the diagnostic.
/// * `status` - The status code of the diagnostic.
/// * `message` - The diagnostic message.
///
/// # Returns
/// A `Result` indicating success or failure.
async fn new_diagnostic(
    db: &SqlitePool,
    token: &str,
    status: i64,
    message: &str,
) -> Result<(), sqlx::Error> {
    sqlx::query("INSERT INTO diagnostics (token, status, message) VALUES (?, ?, ?)")
        .bind(token)
        .bind(status)
        .bind(message)
        .execute(db)
        .await?;

    Ok(())
}
