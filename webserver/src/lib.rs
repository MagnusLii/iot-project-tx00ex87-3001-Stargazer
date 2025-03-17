pub mod api;
pub mod app;
pub mod auth;
pub mod err;
pub mod init;
pub mod keys;
pub mod web;

use sqlx::SqlitePool;
use web::images::ImageDirectory;

/// Shared state struct for the webserver
#[derive(Clone)]
pub struct SharedState {
    /// The API database connection pool
    pub db: SqlitePool,
    /// The image directory struct
    pub image_dir: ImageDirectory,
}
