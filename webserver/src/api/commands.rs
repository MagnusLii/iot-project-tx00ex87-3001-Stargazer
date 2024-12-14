use crate::{api::ApiState, err::Error};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

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

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
struct CommandSql {
    pub id: i64,
    pub target: String,
    pub associated_key: i64,
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
pub struct AllCommandsSql {
    pub target: String,
    pub associated_key: i64,
    pub id: i64,
    pub name: String,
}

pub async fn get_commands(db: &SqlitePool) -> Result<Vec<AllCommandsSql>, Error> {
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
pub struct FetchCommandQuery {
    api_key: String,
}

pub async fn fetch_command(
    State(state): State<ApiState>,
    Query(key): Query<FetchCommandQuery>,
) -> impl IntoResponse {
    println!("Fetching command with: {:?}", key);
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
