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
}

async fn retrieve_command(key: &str, db: &SqlitePool) -> Result<CommandSql, Error> {
    let command = sqlx::query_as(
        "SELECT commands.id AS id, commands.target AS target FROM commands 
        JOIN keys ON commands.associated_key = keys.id
        WHERE keys.api_token = ? AND commands.status = 0",
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

pub async fn modify_command_status(db: &SqlitePool, id: i64, status: i64) {
    sqlx::query("UPDATE commands SET status = ? WHERE id = ?")
        .bind(status)
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

#[derive(Debug, Deserialize)]
pub struct FetchCommandQuery {
    token: String,
}

pub async fn fetch_command(
    State(state): State<ApiState>,
    Query(key): Query<FetchCommandQuery>,
) -> impl IntoResponse {
    println!("Fetching command with: {:?}", key.token);

    match retrieve_command(&key.token, &state.db).await {
        Ok(command) => {
            let json_response = format!(
                r#"{{"target": "{}", "id": {}}}"#,
                command.target, command.id
            );
            modify_command_status(&state.db, command.id, 1).await; // Mark as fetched (status = 1)
            (StatusCode::OK, json_response)
        }
        Err(e) => match e {
            Error::Sqlx(e) => match e {
                sqlx::Error::RowNotFound => {
                    println!("No commands in queue");
                    (StatusCode::OK, "{}".to_string())
                }
                _ => {
                    println!("Sqlx Error: {}", e);
                    (StatusCode::INTERNAL_SERVER_ERROR, "{}".to_string())
                }
            },
            _ => {
                println!("Other Error: {}", e);
                (StatusCode::INTERNAL_SERVER_ERROR, "{}".to_string())
            }
        },
    }
}
