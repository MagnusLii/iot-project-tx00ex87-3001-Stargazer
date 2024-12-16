use crate::{api::ApiState, err::Error};
use axum::{
    extract::{Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
struct CommandSql {
    pub id: i64,
    pub target: String,
    pub associated_key: i64,
}

async fn retrieve_command(key: &str, db: &SqlitePool) -> Result<CommandSql, Error> {
    let command = sqlx::query_as(
        "SELECT commands.id AS id, commands.target AS target , commands.associated_key AS associated_key FROM commands 
        JOIN keys ON commands.associated_key = keys.id
        WHERE keys.api_token = ?",
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
            delete_command(&state.db, command.id).await; // TODO: Maybe mark as fetched instead?
            (StatusCode::OK, command.target)
        }
        Err(e) => {
            println!("Fetch command error: {}", e);
            (StatusCode::INTERNAL_SERVER_ERROR, "Error\n".to_string())
        }
    }
}
