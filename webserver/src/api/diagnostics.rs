use crate::{api::ApiState, keys::verify_key};
use axum::{
    extract::{Json, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::Deserialize;
use sqlx::SqlitePool;

// TODO: Exact fields still TBD
#[derive(Deserialize)]
pub struct DiagnosticsJson {
    key: String,
    status: String,
    message: String,
}

pub async fn send_diagnostics(
    State(state): State<ApiState>,
    Json(payload): Json<DiagnosticsJson>,
) -> impl IntoResponse {
    if !verify_key(&payload.key, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    println!(
        "New diagnostic data: [{}] {}",
        payload.status, payload.message
    );
    new_diagnostic(&state.db, &payload.key, &payload.status, &payload.message).await;

    (StatusCode::OK, "Success\n")
}

async fn new_diagnostic(db: &SqlitePool, key: &str, status: &str, message: &str) {
    sqlx::query("INSERT INTO diagnostics (key, status, message) VALUES (?, ?, ?)")
        .bind(key)
        .bind(status)
        .bind(message)
        .execute(db)
        .await
        .unwrap();
}
