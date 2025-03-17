use crate::{err::Error, SharedState};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

/// Represents a command stored in the database.
#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
struct CommandSql {
    pub id: i64,
    pub target: i64,
    pub position: i64,
}

/// Retrieves a command from the database that matches the provided API token.
/// The command must have a status of 0 (not yet fetched).
///
/// # Arguments
/// * `key` - The API token associated with the command.
/// * `db` - A reference to the SQLite connection pool.
///
/// # Returns
/// A `Result` containing the `CommandSql` if found, or an `Error` if not.
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

/// Modifies the status of a command in the database.
///
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// * `id` - The ID of the command to modify.
/// * `status` - The new status to set.
pub async fn modify_command_status(db: &SqlitePool, id: i64, status: i64) {
    sqlx::query("UPDATE commands SET status = ? WHERE id = ?")
        .bind(status)
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

/// Updates the estimated execution time for a command.
///
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// * `id` - The ID of the command to update.
/// * `time` - The estimated execution time.
pub async fn update_command_est_time(db: &SqlitePool, id: i64, time: i64) {
    sqlx::query("UPDATE commands SET time = ? WHERE id = ?")
        .bind(time)
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

/// Query parameters for fetching a command.
#[derive(Debug, Deserialize)]
pub struct FetchCommandQuery {
    token: String,
}

/// Fetches the next available command for a given API token.
///
/// # Arguments
/// * `state` - The shared application state containing the database pool.
/// * `key` - The query parameters containing an API token used to authenticate the request.
///
/// # Returns
/// A HTTP response containing the command details or an empty response if no command is available.
pub async fn fetch_command(
    State(state): State<SharedState>,
    Query(key): Query<FetchCommandQuery>,
) -> impl IntoResponse {
    println!("Fetching command with: {:?}", key.token);

    match retrieve_command(&key.token, &state.db).await {
        Ok(command) => {
            let json_response = format!(
                r#"{{"target": {}, "position": {}, "id": {}}}"#,
                command.target, command.position, command.id
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

/// Represents the status of a command.
#[derive(Debug, Deserialize, FromRow)]
pub struct CheckCommandSql {
    status: i64,
}

/// Checks the status of a specific command for a given API token.
///
/// # Arguments
/// * `db` - A reference to the SQLite connection pool.
/// * `token` - The API token associated with the command.
/// * `id` - The ID of the command to check.
///
/// # Returns
/// A `Result` containing the command status if found, or an `Error` if not.
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

/// JSON structure for responding to a command.
#[derive(Debug, Deserialize)]
pub struct ResponseCommandJson {
    token: String,
    id: i64,
    status: i64,       // Command stage/status
    time: Option<i64>, // Estimated execution time (if applicable)
}

/// Handles responses to commands, updating their status and estimated execution time.
///
/// # Arguments
/// * `state` - The shared application state containing the database pool.
/// * `response` - The JSON payload containing the command response details.
///
/// # Returns
/// A HTTP response indicating success or failure.
pub async fn respond_command(
    State(state): State<SharedState>,
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
