use crate::sg::{err::Error, images};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};
use std::fs;

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct ApiKey {
    pub id: i64,
    pub api_key: String,
    pub name: String,
}

#[derive(Clone)]
pub struct ApiState {
    pub db: SqlitePool,
}

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

pub async fn get_api_keys(db: &SqlitePool) -> Result<Vec<ApiKey>, Error> {
    let api_keys: Vec<ApiKey> = sqlx::query_as("SELECT * FROM api_keys")
        .fetch_all(db)
        .await?;

    Ok(api_keys)
}

pub async fn api_keys(State(state): State<ApiState>) -> impl IntoResponse {
    let mut html = include_str!("../../html/keys.html").to_string();
    let html_keys = get_api_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| {
            format!(
                "<li value=\"{}\">{}: {}</li><button onclick=\"deleteKey({})\">Delete</button>",
                key.id, key.name, key.api_key, key.id
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    (StatusCode::OK, Html(html))
}

async fn create_api_key(db: &SqlitePool, name: String) -> String {
    let api_key = uuid::Uuid::new_v4().to_string();
    sqlx::query("INSERT INTO api_keys (api_key, name) VALUES (?, ?)")
        .bind(&api_key)
        .bind(name)
        .execute(db)
        .await
        .unwrap();
    api_key
}

async fn delete_api_key(db: &SqlitePool, id: i64) {
    sqlx::query("DELETE FROM api_keys WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

#[derive(Deserialize)]
pub struct NewKey {
    name: String,
}

pub async fn new_key(State(state): State<ApiState>, Json(name): Json<NewKey>) -> impl IntoResponse {
    let key = create_api_key(&state.db, name.name).await;

    (StatusCode::OK, Html(key))
}

#[derive(Deserialize)]
pub struct DeleteKey {
    id: i64,
}

pub async fn delete_key(
    State(state): State<ApiState>,
    Query(delete_key): Query<DeleteKey>,
) -> impl IntoResponse {
    println!("Deleting key: {}", delete_key.id);
    delete_api_key(&state.db, delete_key.id).await;

    (StatusCode::OK, "Success\n")
}

pub async fn verify_key(key: &str, db: &SqlitePool) -> bool {
    let api_keys = get_api_keys(db).await.unwrap();
    for api_key in api_keys {
        if api_key.api_key == key {
            return true;
        }
    }
    false
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

async fn create_command(db: &SqlitePool, command: &str, associated_key: i64) -> Result<(), Error> {
    sqlx::query("INSERT INTO commands (target, associated_key) VALUES (?, ?)")
        .bind(command)
        .bind(associated_key)
        .execute(db)
        .await
        .unwrap();
    Ok(())
}

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct CommandJson {
    target: String,
    associated_key_id: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct CommandSql {
    pub id: i64,
    pub target: String,
    pub associated_key: i64,
}

pub async fn new_command(
    State(state): State<ApiState>,
    Json(payload): Json<CommandJson>,
) -> impl IntoResponse {
    println!(
        "Creating command: {}, associated key: {}",
        payload.target, payload.associated_key_id
    );
    let int_key: i64 = payload.associated_key_id.parse().unwrap();

    create_command(&state.db, &payload.target, int_key)
        .await
        .unwrap();
    (StatusCode::OK, "Success\n")
}

async fn retrieve_command(key: &str, db: &SqlitePool) -> Result<CommandSql, Error> {
    let command = sqlx::query_as(
        "SELECT commands.id AS id, commands.target AS target , commands.associated_key AS associated_key FROM commands 
        JOIN api_keys ON commands.associated_key = api_keys.id
        WHERE api_keys.api_key = ?",
    )
    .bind(key)
    .fetch_one(db)
    .await?;

    Ok(command)
}

async fn delete_command(db: &SqlitePool, id: i64) {
    println!("Deleting command: {}", id);
    sqlx::query("DELETE FROM commands WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct AllCommandSql {
    pub target: String,
    pub associated_key: i64,
    pub id: i64,
    pub name: String,
}

pub async fn get_commands(db: &SqlitePool) -> Result<Vec<AllCommandSql>, Error> {
    let commands = sqlx::query_as(
        "SELECT commands.id AS id, 
        commands.target AS target, 
        commands.associated_key AS associated_key, 
        api_keys.name as name 
        FROM commands 
        JOIN api_keys ON commands.associated_key = api_keys.id",
    )
    .fetch_all(db)
    .await?;
    Ok(commands)
}

#[derive(Debug, Deserialize)]
pub struct FetchCommand {
    api_key: String,
}

pub async fn fetch_command(
    State(state): State<ApiState>,
    Query(key): Query<FetchCommand>,
) -> impl IntoResponse {
    println!("Fetching command: {:?}", key);
    // Retrieve command and if not errors occur, delete the command
    match retrieve_command(&key.api_key, &state.db).await {
        Ok(command) => {
            delete_command(&state.db, command.id).await;
            (StatusCode::OK, command.target)
        }
        Err(e) => {
            println!("Fetch command error: {}", e);
            (StatusCode::INTERNAL_SERVER_ERROR, "Error\n".to_string())
        }
    }
}

#[derive(Deserialize)]
pub struct UploadImage {
    key: String,
    data: String,
}

// Parse the body of the POST request for base64 encoded image
// TODO: Handle errors
pub async fn upload(
    State(state): State<ApiState>,
    Json(payload): Json<UploadImage>,
) -> impl IntoResponse {
    if !verify_key(&payload.key, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    let data = payload.data;
    //println!("{}", data);

    let decoded = general_purpose::STANDARD.decode(data).unwrap();
    assert!(infer::is_image(&decoded));

    let file_info = infer::get(&decoded).expect("file type is known");
    println!("{:?}", file_info.mime_type());

    let now = Utc::now().timestamp();

    // Write path, TODO: Can technically be a collision
    let path = format!("./images/{}.{}", now, file_info.extension());

    fs::write(path, decoded).unwrap();

    images::update_gallery().await;

    (StatusCode::OK, "Success\n")
}

// TODO: Exact fields still TBD
#[derive(Deserialize)]
pub struct DiagnosticsJson {
    key: String,
    status: String,
    message: String,
}

pub async fn send_diagnostics(
    State(state): State<ApiState>,
    Json(payload): Json<DiagnosticsJson>,
) -> impl IntoResponse {
    if !verify_key(&payload.key, &state.db).await {
        return (StatusCode::UNAUTHORIZED, "Unauthorized\n");
    }

    println!(
        "New diagnostic data: [{}] {}",
        payload.status, payload.message
    );
    new_diagnostic(&state.db, &payload.key, &payload.status, &payload.message).await;

    (StatusCode::OK, "Success\n")
}

async fn new_diagnostic(db: &SqlitePool, key: &str, status: &str, message: &str) {
    sqlx::query("INSERT INTO diagnostics (key, status, message) VALUES (?, ?, ?)")
        .bind(key)
        .bind(status)
        .bind(message)
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

pub async fn time() -> impl IntoResponse {
    let now = Utc::now().timestamp();
    (StatusCode::OK, now.to_string())
}
