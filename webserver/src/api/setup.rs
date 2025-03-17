use sqlx::SqlitePool;

/// Creates the `keys` table if it does not exist.
/// This table stores API keys associated with users or services.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `api_token` - Unique API token (text, required)
/// * `name` - Optional name associated with the API key
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

/// Creates the `commands` table if it does not exist.
/// This table stores commands issued by clients.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `target` - Foreign key referencing `objects(id)`
/// * `position` - Foreign key referencing `object_positions(id)`
/// * `associated_key` - Foreign key referencing `keys(id)`
/// * `status` - Status of the command (integer, defaults to 0)
/// * `time` - Optional estimated execution time (integer)
pub async fn create_command_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS commands (
            id INTEGER PRIMARY KEY,
            target INTEGER NOT NULL,
            position INTEGER NOT NULL,
            associated_key INTEGER NOT NULL,
            status INTEGER NOT NULL DEFAULT 0,
            time INTEGER,
            FOREIGN KEY (target) REFERENCES objects (id),
            FOREIGN KEY (position) REFERENCES object_positions (id),
            FOREIGN KEY (associated_key) REFERENCES keys (id)
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

/// Creates the `diagnostics` table if it does not exist.
/// This table stores diagnostic messages from clients.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `token` - Foreign key referencing `keys(api_token)`
/// * `status` - Foreign key referencing `diagnostics_status(id)`
/// * `message` - Diagnostic message (text)
/// * `time` - Timestamp of entry (defaults to current Unix time)
pub async fn create_diagnostics_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS diagnostics (
            id INTEGER PRIMARY KEY,
            token TEXT NOT NULL,
            status INTEGER NOT NULL,
            message TEXT,
            time INTEGER DEFAULT (unixepoch('now')),
            FOREIGN KEY (token) REFERENCES keys (api_token)
            FOREIGN KEY (status) REFERENCES diagnostics_status (id)   
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

/// Creates the `images` table if it does not exist.
/// This table stores image metadata related to commands.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `name` - Image name (text, required)
/// * `path` - Unique file path to the image (text, required)
/// * `web_path` - Unique web-accessible path (text, required)
/// * `command_id` - Foreign key referencing `commands(id)`
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

/// Creates the `objects` table if it does not exist.
/// This table stores astronomical objects.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `name` - Object name (text, required)
pub async fn create_objects_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS objects (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL
        )",
    )
    .execute(db)
    .await
    .unwrap();

    populate_objects_table(db).await;
}

/// Populates the `objects` table with predefined celestial objects.
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
async fn populate_objects_table(db: &SqlitePool) {
    sqlx::query(
        "INSERT INTO objects VALUES 
                (1, 'Sun'),
                (2, 'Moon'),
                (3, 'Mercury'),
                (4, 'Venus'),
                (5, 'Mars'),
                (6, 'Jupiter'),
                (7, 'Saturn'),
                (8, 'Uranus'),
                (9, 'Neptune')",
    )
    .execute(db)
    .await
    .unwrap();
}

/// Creates the `object_positions` table if it does not exist.
/// This table stores predefined object positions.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `position` - Position name (text, required)
pub async fn create_position_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS object_positions (
            id INTEGER PRIMARY KEY,
            position TEXT NOT NULL
        )",
    )
    .execute(db)
    .await
    .unwrap();

    populate_position_table(db).await;
}

/// Populates the `object_positions` table with predefined positions.
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
async fn populate_position_table(db: &SqlitePool) {
    sqlx::query(
        "INSERT INTO object_positions VALUES 
        (1, 'Rising'),
        (2, 'Zenith'),
        (3, 'Setting'),
        (4, 'Any')",
    )
    .execute(db)
    .await
    .unwrap();
}

/// Creates the `diagnostics_status` table if it does not exist.
/// This table stores predefined diagnostic status levels.
/// 
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// 
/// # Columns
/// * `id` - Primary key (integer, auto-incremented)
/// * `status` - Status name (text, required)
pub async fn create_diagnostics_status_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS diagnostics_status (
            id INTEGER PRIMARY KEY,
            status TEXT NOT NULL
        )",
    )
    .execute(db)
    .await
    .unwrap();

    populate_diagnostics_status_table(db).await;
}

/// Populates the `diagnostics_status` table with predefined statuses.
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
async fn populate_diagnostics_status_table(db: &SqlitePool) {
    sqlx::query(
        "INSERT INTO diagnostics_status VALUES 
        (1, 'Info'),
        (2, 'Warning'),
        (3, 'Error')",
    )
    .execute(db)
    .await
    .unwrap();
}
