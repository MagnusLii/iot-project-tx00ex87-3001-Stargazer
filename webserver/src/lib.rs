pub mod api;
pub mod app;
pub mod auth;
pub mod err;
pub mod init;
pub mod keys;
pub mod web;

use sqlx::SqlitePool;
use web::images::ImageDirectory;

#[derive(Clone)]
pub struct SharedState {
    pub db: SqlitePool,
    pub image_dir: ImageDirectory,
}
