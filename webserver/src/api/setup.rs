use sqlx::SqlitePool;

pub async fn create_api_keys_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS keys (
            id INTEGER PRIMARY KEY,
            api_token TEXT NOT NULL UNIQUE,
            name TEXT
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn create_command_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS commands (
            id INTEGER PRIMARY KEY,
            target TEXT NOT NULL,
            position TEXT NOT NULL,
            associated_key INTEGER NOT NULL,
            status INTEGER NOT NULL DEFAULT 0,
            time INTEGER,
            FOREIGN KEY (associated_key) REFERENCES keys (id)
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn create_diagnostics_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS diagnostics (
            id INTEGER PRIMARY KEY,
            token TEXT NOT NULL,
            status TEXT NOT NULL,
            message TEXT,
            FOREIGN KEY (token) REFERENCES keys (api_token)
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn create_image_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS images (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            path TEXT NOT NULL UNIQUE,
            web_path TEXT NOT NULL UNIQUE,
            command_id INTEGER NOT NULL,
            FOREIGN KEY (command_id) REFERENCES commands (id)
        )",
    )
    .execute(db)
    .await
    .unwrap();
}
