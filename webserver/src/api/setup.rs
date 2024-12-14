use sqlx::SqlitePool;

pub async fn create_api_keys_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS api_keys (
            id INTEGER PRIMARY KEY,
            api_key TEXT NOT NULL UNIQUE,
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
            associated_key INTEGER NOT NULL,
            FOREIGN KEY (associated_key) REFERENCES api_keys (id)
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
            key TEXT NOT NULL,
            status TEXT NOT NULL,
            message TEXT,
            FOREIGN KEY (key) REFERENCES api_keys (api_key)
        )",
    )
    .execute(db)
    .await
    .unwrap();
}
