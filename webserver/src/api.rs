pub mod commands;
pub mod diagnostics;
pub mod setup;
pub mod time_srv;
pub mod upload;

use sqlx::SqlitePool;

#[derive(Clone)]
pub struct ApiState {
    pub db: SqlitePool,
}
