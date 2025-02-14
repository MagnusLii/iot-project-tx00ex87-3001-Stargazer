use crate::{keys::verify_key, SharedState};
use axum::{
    extract::{Json, State},
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
    Json(payload): Json<DiagnosticsJson>,
) -> impl IntoResponse {
    if !verify_key(&payload.token, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    println!(
        "New diagnostic data: [{}] {}",
        payload.status, payload.message
    );
    new_diagnostic(&state.db, &payload.token, payload.status, &payload.message).await;

    (StatusCode::OK, "Success\n")
}

async fn new_diagnostic(db: &SqlitePool, token: &str, status: i64, message: &str) {
    sqlx::query("INSERT INTO diagnostics (token, status, message) VALUES (?, ?, ?)")
        .bind(token)
        .bind(status)
        .bind(message)
        .execute(db)
        .await
        .unwrap();
}
