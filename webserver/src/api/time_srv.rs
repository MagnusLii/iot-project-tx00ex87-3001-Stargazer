use axum::{http::StatusCode, response::IntoResponse};
use chrono::Utc;

pub async fn time() -> impl IntoResponse {
    let now = Utc::now().timestamp();
    (StatusCode::OK, now.to_string())
}
