use crate::{api::ApiState, err::Error};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
struct CommandSql {
    pub id: i64,
    pub target: String,
    pub position: String,
}

async fn retrieve_command(key: &str, db: &SqlitePool) -> Result<CommandSql, Error> {
    let command = sqlx::query_as(
        "SELECT commands.id AS id, commands.target AS target, commands.position AS position FROM commands 
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
            let position: i64;
            match command.position.as_str() {
                "rising" => {
                    position = 1;
                }
                "zenith" => {
                    position = 2;
                }
                "setting" => {
                    position = 3;
                }
                _ => {
                    position = 0;
                    println!("Invalid position: {}", command.position);
                    modify_command_status(&state.db, command.id, -9).await; // Mark as failed internally (status = -9)
                }
            }

            if position == 0 {
                return (StatusCode::INTERNAL_SERVER_ERROR, "{}".to_string());
            }

            let json_response = format!(
                r#"{{"target": "{}", "position": {}, "id": {}}}"#,
                command.target, position, command.id
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

#[derive(Debug, Deserialize, FromRow)]
pub struct CheckCommandSql {
    status: i64,
}

async fn check_command_status(
    db: &SqlitePool,
    token: &str,
    id: i64,
) -> Result<CheckCommandSql, Error> {
    let command = sqlx::query_as(
        "SELECT commands.status AS status FROM commands 
        JOIN keys ON commands.associated_key = keys.id
        WHERE keys.api_token = ? AND commands.id = ?",
    )
    .bind(token)
    .bind(id)
    .fetch_one(db)
    .await?;

    Ok(command)
}

#[derive(Debug, Deserialize)]
pub struct ResponseCommandJson {
    token: String,
    id: i64,
    response: bool,
}

pub async fn respond_command(
    State(state): State<ApiState>,
    Json(response): Json<ResponseCommandJson>,
) -> impl IntoResponse {
    println!(
        "Response command id: {} with: {}",
        response.id, response.response
    );

    match check_command_status(&state.db, &response.token, response.id).await {
        Ok(command) => {
            if command.status == 1 {
                // We expect response at this stage on both success and failure
                modify_command_status(
                    &state.db,
                    response.id,
                    if response.response { 2 } else { -1 },
                )
                .await;
                (StatusCode::OK, "{}".to_string())
            } else if command.status == 2 {
                // We only expect a response at this stage on failure
                if !response.response {
                    modify_command_status(&state.db, response.id, -2).await;
                }
                (StatusCode::OK, "{}".to_string())
            } else {
                println!("Not expecting response to command");
                (StatusCode::BAD_REQUEST, "{}".to_string())
            }
        }
        Err(e) => match e {
            Error::Sqlx(e) => match e {
                sqlx::Error::RowNotFound => {
                    println!("Error: Command not found");
                    (StatusCode::BAD_REQUEST, "{}".to_string())
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
