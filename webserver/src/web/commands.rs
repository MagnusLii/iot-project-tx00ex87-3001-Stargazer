use crate::{api::commands::modify_command_status, err::Error, SharedState};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

async fn create_command(
    db: &SqlitePool,
    command: i64,
    position: i64,
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
    target: i64,
    position: i64,
    associated_key_id: i64,
}

pub async fn new_command(
    State(state): State<SharedState>,
    Json(payload): Json<CommandJson>,
) -> impl IntoResponse {
    println!(
        "Creating command: {} at position: {}, associated key: {}",
        payload.target, payload.position, payload.associated_key_id
    );
    //let int_key: i64 = payload.associated_key_id.parse().unwrap();

    create_command(
        &state.db,
        payload.target,
        payload.position,
        payload.associated_key_id,
    )
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
    pub datetime: String,
}

pub async fn get_commands(db: &SqlitePool) -> Result<Vec<MultipleCommandSql>, Error> {
    let commands = sqlx::query_as(
        "SELECT commands.id AS id, 
        objects.name AS target, 
        object_positions.position AS position,
        commands.associated_key AS associated_key, 
        commands.status AS status,
        datetime(commands.time, 'unixepoch') AS datetime,
        keys.name as name 
        FROM commands 
        JOIN objects ON commands.target = objects.id
        JOIN object_positions ON commands.position = object_positions.id
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
    State(state): State<SharedState>,
    Query(payload): Query<RemoveCommandQuery>,
) -> impl IntoResponse {
    println!("Marking command as deleted: {}", payload.id);
    let int_key: i64 = payload.id.parse().unwrap();
    modify_command_status(&state.db, int_key, -6).await; // Mark as deleted (status = -6)

    (StatusCode::OK, "Success\n")
}

#[derive(Debug, Clone, Deserialize, FromRow)]
pub struct TargetNameSql {
    pub id: i64,
    pub name: String,
}

pub async fn get_target_names(db: &SqlitePool) -> Result<Vec<TargetNameSql>, Error> {
    let targets = sqlx::query_as("SELECT * FROM objects")
        .fetch_all(db)
        .await?;

    Ok(targets)
}

#[derive(Debug, Clone, Deserialize, FromRow)]
pub struct TargetPosSql {
    pub id: i64,
    pub position: String,
}

pub async fn get_target_positions(db: &SqlitePool) -> Result<Vec<TargetPosSql>, Error> {
    let positions = sqlx::query_as("SELECT * FROM object_positions")
        .fetch_all(db)
        .await?;

    Ok(positions)
}

#[derive(Debug, Clone, Deserialize, FromRow)]
pub struct NextPicEstimateSql {
    pub target: String,
    pub datetime: String,
}

pub async fn get_next_pic_estimate(db: &SqlitePool) -> Result<NextPicEstimateSql, Error> {
    let estimate = sqlx::query_as(
        "SELECT objects.name AS target, datetime(commands.time, 'unixepoch') AS datetime
        FROM commands 
        JOIN objects ON commands.target = objects.id
        WHERE commands.status = 2 AND commands.time IS NOT NULL
        ORDER BY commands.time",
    )
    .fetch_one(db)
    .await?;

    Ok(estimate)
}
