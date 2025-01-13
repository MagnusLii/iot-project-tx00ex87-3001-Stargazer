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
            target INTEGER NOT NULL,
            position TEXT NOT NULL,
            associated_key INTEGER NOT NULL,
            status INTEGER NOT NULL DEFAULT 0,
            time INTEGER,
            FOREIGN KEY (target) REFERENCES objects (id),
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

pub async fn create_objects_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS objects (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn populate_objects_table(db: &SqlitePool) {
    sqlx::query(
        "INSERT INTO objects VALUES 
                (1, 'Sun'),
                (2, 'Moon'),
                (3, 'Mercury'),
                (4, 'Venus'),
                (5, 'Earth'),
                (6, 'Mars'),
                (7, 'Jupiter'),
                (8, 'Saturn'),
                (9, 'Uranus'),
                (10, 'Neptune')",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn create_position_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS object_positions (
            id INTEGER PRIMARY KEY,
            position TEXT NOT NULL,
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn populate_position_table(db: &SqlitePool) {
    sqlx::query(
        "INSERT INTO object_positions VALUES 
        (1, 'Rising'),
        (2, 'Zenith'),
        (3, 'Setting')",
    )
    .execute(db)
    .await
    .unwrap();
}
