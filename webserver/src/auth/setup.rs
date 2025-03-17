#[cfg(feature = "pw_hash")]
use crate::auth::password;

use sqlx::SqlitePool;

/// Creates the `users` table in the database if it does not already exist.
/// 
/// The table includes columns for user ID, username, password, and superuser status.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite database connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `username` - User's username (text, required, unique)
/// * `password` - User's password (text, required)
/// * `superuser` - Superuser status (boolean, required, default 0)
pub async fn create_user_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            username TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL,
            superuser BOOLEAN NOT NULL DEFAULT 0
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

/// Creates an admin user in the `users` table if it does not already exist.
/// 
/// The admin user has the username `admin`, a default password, and superuser status.
/// The password is hashed if the `pw_hash` feature is enabled.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite database connection pool.
pub async fn create_admin(db: &SqlitePool) {
    #[cfg(feature = "pw_hash")]
    let pw = password::generate_phc_string("admin").unwrap();

    #[cfg(not(feature = "pw_hash"))]
    let pw = "admin";

    sqlx::query(
        "INSERT INTO users (username, password, superuser) 
            VALUES ('admin', ?, 1)",
    )
    .bind(&pw)
    .execute(db)
    .await
    .unwrap();
}
