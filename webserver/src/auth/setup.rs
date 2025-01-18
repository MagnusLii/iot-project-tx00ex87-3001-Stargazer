#[cfg(feature = "pw_hash")]
use crate::auth::password;

use sqlx::SqlitePool;

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
