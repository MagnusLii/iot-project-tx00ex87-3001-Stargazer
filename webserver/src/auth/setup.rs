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
    sqlx::query(
        "INSERT INTO users (username, password, superuser) 
            VALUES ('admin', 'admin', 1)",
    )
    .execute(db)
    .await
    .unwrap();
}
