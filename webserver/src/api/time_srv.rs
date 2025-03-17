use axum::{http::StatusCode, response::IntoResponse};
use chrono::Utc;

/// Handles a request for the current time.
/// 
/// # Returns
/// An HTTP response containing the current time as a Unix timestamp.
pub async fn time() -> impl IntoResponse {
    let now = Utc::now().timestamp();
    (StatusCode::OK, now.to_string())
}
