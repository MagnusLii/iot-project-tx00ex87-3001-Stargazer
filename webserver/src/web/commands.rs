use crate::{
    api::{commands::modify_command_status, ApiState},
    err::Error,
};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

async fn create_command(
    db: &SqlitePool,
    command: &str,
    position: &str,
    associated_key: i64,
) -> Result<(), Error> {
    sqlx::query("INSERT INTO commands (target, position, associated_key) VALUES (?, ?, ?)")
        .bind(command)
        .bind(position)
        .bind(associated_key)
        .execute(db)
        .await
        .unwrap();
    Ok(())
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
pub struct CommandJson {
    target: String,
    position: String,
    associated_key_id: String,
}

pub async fn new_command(
    State(state): State<ApiState>,
    Json(payload): Json<CommandJson>,
) -> impl IntoResponse {
    println!(
        "Creating command: {} at position: {}, associated key: {}",
        payload.target, payload.position, payload.associated_key_id
    );
    let int_key: i64 = payload.associated_key_id.parse().unwrap();

    create_command(&state.db, &payload.target, &payload.position, int_key)
        .await
        .unwrap();
    (StatusCode::OK, "Success\n")
}

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct MultipleCommandSql {
    pub target: String,
    pub position: String,
    pub associated_key: i64,
    pub id: i64,
    pub name: String,
    pub status: i64,
    pub time: i64,
}

pub async fn get_commands(db: &SqlitePool) -> Result<Vec<MultipleCommandSql>, Error> {
    let commands = sqlx::query_as(
        "SELECT commands.id AS id, 
        commands.target AS target, 
        commands.position AS position,
        commands.associated_key AS associated_key, 
        commands.status AS status,
        commands.time AS time,
        keys.name as name 
        FROM commands 
        JOIN keys ON commands.associated_key = keys.id",
    )
    .fetch_all(db)
    .await?;

    Ok(commands)
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemoveCommandQuery {
    id: String,
}

pub async fn remove_command(
    State(state): State<ApiState>,
    Query(payload): Query<RemoveCommandQuery>,
) -> impl IntoResponse {
    println!("Marking command as deleted: {}", payload.id);
    let int_key: i64 = payload.id.parse().unwrap();
    modify_command_status(&state.db, int_key, -6).await; // Mark as deleted (status = -6)

    (StatusCode::OK, "Success\n")
}
