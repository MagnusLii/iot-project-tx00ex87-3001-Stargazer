use crate::{keys::verify_key, SharedState};
use axum::{
    extract::{rejection::JsonRejection, Json, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::Deserialize;
use sqlx::SqlitePool;

#[derive(Deserialize)]
pub struct DiagnosticsJson {
    token: String,
    status: i64,
    message: String,
}

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

    if !verify_key(&payload.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized");
    }

    println!(
        "New diagnostic data: [{}] {}",
        payload.status, payload.message
    );
    if let Err(e) =
        new_diagnostic(&state.db, &payload.token, payload.status, &payload.message).await
    {
        eprintln!("Error saving diagnostic: {}", e);
        return (StatusCode::INTERNAL_SERVER_ERROR, "Error saving diagnostic");
    }

    (StatusCode::OK, "Success\n")
}

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
