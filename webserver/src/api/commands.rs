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

pub async fn modify_command_status(db: &SqlitePool, id: i64, status: i64) {
    sqlx::query("UPDATE commands SET status = ? WHERE id = ?")
        .bind(status)
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

pub async fn update_command_est_time(db: &SqlitePool, id: i64, time: i64) {
    sqlx::query("UPDATE commands SET time = ? WHERE id = ?")
        .bind(time)
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
    status: i64, /* Command stage/status, (-)1 = Fetch, (-)2 = Calculate, ((-)3 = Picture)
                  * Negative numbers are errors */
    time: Option<i64>, // Estimated picture time in 2
}

pub async fn respond_command(
    State(state): State<ApiState>,
    Json(response): Json<ResponseCommandJson>,
) -> impl IntoResponse {
    println!(
        "Received response: status {} for command id {}",
        response.status, response.id
    );

    // Don't continue if status is not valid
    if response.status < -3 || response.status > 2 {
        println!("Invalid response status: {}", response.status);
        return (StatusCode::BAD_REQUEST, "{}".to_string());
    }

    match check_command_status(&state.db, &response.token, response.id).await {
        Ok(command) => {
            if command.status < 0 || command.status > 2 {
                println!("Unexpected response to command: {}", response.id);
                return (StatusCode::BAD_REQUEST, "{}".to_string());
            }
            println!(
                "Command id {} status: {} -> {}",
                response.id, command.status, response.status
            );

            modify_command_status(&state.db, response.id, response.status).await;
            if response.status == 2 {
                match response.time {
                    Some(n_time) => update_command_est_time(&state.db, response.id, n_time).await,
                    None => println!("No time estimate provided"),
                }
            }

            (StatusCode::OK, "{}".to_string())
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
