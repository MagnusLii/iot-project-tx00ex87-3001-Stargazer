use crate::{api::ApiState, err::Error};
use axum::{
    extract::{Json, State},
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
pub struct MultipleCommandSql {
    pub target: String,
    pub associated_key: i64,
    pub id: i64,
    pub name: String,
}

pub async fn get_commands(db: &SqlitePool) -> Result<Vec<MultipleCommandSql>, Error> {
    let commands = sqlx::query_as(
        "SELECT commands.id AS id, 
        commands.target AS target, 
        commands.associated_key AS associated_key, 
        keys.name as name 
        FROM commands 
        JOIN keys ON commands.associated_key = keys.id",
    )
    .fetch_all(db)
    .await?;
    Ok(commands)
}
