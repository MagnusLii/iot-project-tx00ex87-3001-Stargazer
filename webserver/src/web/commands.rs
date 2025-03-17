use crate::{api::commands::modify_command_status, err::Error, SharedState};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

/// Inserts a new command into the database.
///
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `command`: The target ID of the command.
/// - `position`: The position ID of the command.
/// - `associated_key`: The associated key ID of the command.
///
/// # Returns
/// - `Ok(())` if the command is successfully created.
/// - `Err(Error)` if an error occurs.
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

/// Deletes a command from the database by its ID.
///
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `id`: The ID of the command to delete.
#[allow(dead_code)]
async fn delete_command(db: &SqlitePool, id: i64) {
    println!("Deleting command: {}", id);
    sqlx::query("DELETE FROM commands WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

/// Represents the JSON structure for a command.
#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct CommandJson {
    /// The target ID of the command.
    target: i64,
    /// The position ID of the command.
    position: i64,
    /// The associated key ID of the command.
    associated_key_id: i64,
}

/// Handles the creation of a new command via an HTTP request.
///
/// # Arguments
/// - `state`: The shared application state.
/// - `payload`: The JSON payload containing the command details.
///
/// # Returns
/// - A HTTP response indicating the success or failure of the operation.
pub async fn new_command(
    State(state): State<SharedState>,
    Json(payload): Json<CommandJson>,
) -> impl IntoResponse {
    println!(
        "Creating command: {} at position: {}, associated key: {}",
        payload.target, payload.position, payload.associated_key_id
    );

    if payload.target < 1 || payload.target > 9 {
        return (StatusCode::BAD_REQUEST, "Invalid target");
    }

    if payload.position < 1 || payload.position > 4 {
        return (StatusCode::BAD_REQUEST, "Invalid position");
    }

    create_command(
        &state.db,
        payload.target,
        payload.position,
        payload.associated_key_id,
    )
    .await
    .unwrap();

    (StatusCode::OK, "Success")
}

/// Represents a command for database queries.
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

impl MultipleCommandSql {
    /// Converts the command into a JSON string representation.
    pub fn to_json(&self) -> String {
        format!(
            r#"{{"target": "{}", "position": "{}", "id": {}, "name": "{}", "key_id": {}, "status": {}, "datetime": "{}"}}"#,
            self.target,
            self.position,
            self.id,
            self.name,
            self.associated_key,
            self.status,
            self.datetime
        )
    }
}

/// Fetches a paginated list of commands from the database.
///
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `page`: The page number for pagination.
///
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
pub async fn get_commands(db: &SqlitePool, page: i64) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        LIMIT ? OFFSET ?",
    )
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Fetches a paginated list of commands from the database by status.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `status`: The status of the commands to fetch.
/// - `page`: The page number for pagination.
/// 
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
/// - An `Error` if an error occurs.
pub async fn get_commands_by_status(
    db: &SqlitePool,
    status: i64,
    page: i64,
) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        WHERE commands.status = ?
        LIMIT ? OFFSET ?",
    )
    .bind(status)
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Fetches a paginated list of commands from the database by key.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `key`: The key of the commands to fetch.
/// - `page`: The page number for pagination.
/// 
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
/// - An `Error` if an error occurs.
pub async fn get_commands_by_key(
    db: &SqlitePool,
    key: i64,
    page: i64,
) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        WHERE commands.associated_key = ?
        LIMIT ? OFFSET ?",
    )
    .bind(key)
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Fetches a paginated list of completed commands from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `page`: The page number for pagination.
/// 
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
/// - An `Error` if an error occurs. 
pub async fn get_completed_commands(
    db: &SqlitePool,
    page: i64,
) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        WHERE commands.status = 3
        LIMIT ? OFFSET ?",
    )
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Fetches a paginated list of failed commands from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `page`: The page number for pagination.
/// 
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
/// - An `Error` if an error occurs.
pub async fn get_failed_commands(
    db: &SqlitePool,
    page: i64,
) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        WHERE commands.status BETWEEN -5 AND -1
        LIMIT ? OFFSET ?",
    )
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Fetches a paginated list of in-progress commands from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `page`: The page number for pagination.
/// 
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
/// - An `Error` if an error occurs.
pub async fn get_in_progress_commands(
    db: &SqlitePool,
    page: i64,
) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        WHERE commands.status BETWEEN 0 AND 2
        LIMIT ? OFFSET ?",
    )
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Fetches a paginated list of deleted commands from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `page`: The page number for pagination.
/// 
/// # Returns
/// - A vector of `MultipleCommandSql` representing the commands.
/// - An `Error` if an error occurs.
pub async fn get_deleted_commands(
    db: &SqlitePool,
    page: i64,
) -> Result<Vec<MultipleCommandSql>, Error> {
    let limit = 25;
    let offset = page * limit;

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
        JOIN keys ON commands.associated_key = keys.id
        WHERE commands.status = -6
        LIMIT ? OFFSET ?",
    )
    .bind(limit)
    .bind(offset)
    .fetch_all(db)
    .await?;

    Ok(commands)
}

/// Represents the count of commands by status.
#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct CommandCount {
    pub count: i64,
    pub status: i64,
}

/// Fetches the total count of all commands.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_all_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands")
        .fetch_one(db)
        .await?;

    Ok(count)
}

/// Fetches the total count of completed commands.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_completed_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE status = 3")
        .fetch_one(db)
        .await?;

    Ok(count)
}

/// Fetches the total count of failed commands.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_failed_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as(
        "SELECT COUNT(*) AS count, status FROM commands WHERE status BETWEEN -5 AND -1",
    )
    .fetch_one(db)
    .await?;

    Ok(count)
}

/// Fetches the total count of in-progress commands.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_in_progress_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as(
        "SELECT COUNT(*) AS count, status FROM commands WHERE status BETWEEN 0 AND 2",
    )
    .fetch_one(db)
    .await?;

    Ok(count)
}

/// Fetches the total count of deleted commands.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_deleted_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE status = -6")
        .fetch_one(db)
        .await?;

    Ok(count)
}

/// Fetches the total count of commands by status.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `status`: The status of the commands to count.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_commands_count_by_status(db: &SqlitePool, status: i64) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE status = ?")
        .bind(status)
        .fetch_one(db)
        .await?;

    Ok(count)
}

/// Fetches the total count of commands by key.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `key`: The key of the commands to count.
/// 
/// # Returns
/// - A `CommandCount` representing the count of commands and the status.
/// - An `Error` if an error occurs.
async fn get_commands_count_by_key(db: &SqlitePool, key: i64) -> Result<CommandCount, Error> {
    let count =
        sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE associated_key = ?")
            .bind(key)
            .fetch_one(db)
            .await?;

    Ok(count)
}

/// Represents the query parameters for the remove command endpoint.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemoveCommandQuery {
    /// The ID of the command to remove.
    id: String,
}

/// Handles the removal of a command via an HTTP request.
/// 
/// # Arguments
/// - `state`: The shared application state.
/// - `payload`: The query parameters containing the command ID.
/// 
/// # Returns
/// - A HTTP response indicating the success or failure of the operation.
pub async fn remove_command(
    State(state): State<SharedState>,
    Query(payload): Query<RemoveCommandQuery>,
) -> impl IntoResponse {
    println!("Marking command as deleted: {}", payload.id);
    let int_key: i64 = payload.id.parse().unwrap();
    modify_command_status(&state.db, int_key, -6).await; // Mark as deleted (status = -6)

    (StatusCode::OK, "Success")
}

/// Represents the target celestial object names in the database.
#[derive(Debug, Clone, Deserialize, FromRow)]
pub struct TargetNameSql {
    /// The ID of the celestial object.
    pub id: i64,
    /// The name of the celestial object.
    pub name: String,
}

/// Fetches the list of valid target names from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A vector of `TargetNameSql` representing the target names.
/// - An `Error` if an error occurs.
pub async fn get_target_names(db: &SqlitePool) -> Result<Vec<TargetNameSql>, Error> {
    let targets = sqlx::query_as("SELECT * FROM objects")
        .fetch_all(db)
        .await?;

    Ok(targets)
}

/// Represents the target celestial object position names in the database.
#[derive(Debug, Clone, Deserialize, FromRow)]
pub struct TargetPosSql {
    /// The ID of the celestial object position.
    pub id: i64,
    /// The name of the celestial object position.
    pub position: String,
}

/// Fetches the list of valid target position names from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A vector of `TargetPosSql` representing the target positions.
/// - An `Error` if an error occurs.
pub async fn get_target_positions(db: &SqlitePool) -> Result<Vec<TargetPosSql>, Error> {
    let positions = sqlx::query_as("SELECT * FROM object_positions")
        .fetch_all(db)
        .await?;

    Ok(positions)
}

/// Represents the next picture estimate based on the information in the database.
#[derive(Debug, Clone, Deserialize, FromRow)]
pub struct NextPicEstimateSql {
    /// The name of the target celestial object.
    pub target: String,
    /// The estimated date and time of the next picture.
    pub datetime: String,
}

/// Fetches the estimated date and time of the next picture based on the information in the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `NextPicEstimateSql` representing the next picture estimate.
/// - An `Error` if an error occurs.
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

/// Represents the parameters that will be used to query the database for commands.
#[derive(Debug, Clone, Deserialize)]
pub struct RequestCommandsJson {
    /// The filter type to use.
    pub filter_type: i64, /* 0 = all, 1 = completed, 2 = in progress, 3 = failed, 4 = deleted,
                           * 5 = specific status, 6 = specific key (id)
                           */
    /// The filter value to use.
    pub filter_value: Option<i64>,
    /// The page number for pagination.
    pub page: Option<i64>,
}

/// Handles the request for command information via an HTTP request.
///
/// # Arguments
/// - `state`: The shared application state.
/// - `payload`: The JSON payload containing the command query details.
/// 
/// # Returns
/// - A HTTP response containing the command information or Error status code.
#[axum::debug_handler]
pub async fn request_commands_info(
    State(state): State<SharedState>,
    Json(payload): Json<RequestCommandsJson>,
) -> impl IntoResponse {
    dbg!(&payload);

    let page = payload.page.unwrap_or(0);
    let mut json_response: String = "{\"commands\":[".to_string();
    let total_count: i64;
    let commands: Vec<MultipleCommandSql>;
    let mut remaining: usize;

    match payload.filter_type {
        0 => {
            commands = get_commands(&state.db, page).await.unwrap();
            remaining = commands.len();

            total_count = get_all_commands_count(&state.db)
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        1 => {
            commands = get_completed_commands(&state.db, page).await.unwrap();
            remaining = commands.len();

            total_count = get_completed_commands_count(&state.db)
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        2 => {
            commands = get_in_progress_commands(&state.db, page).await.unwrap();
            remaining = commands.len();

            total_count = get_in_progress_commands_count(&state.db)
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        3 => {
            commands = get_failed_commands(&state.db, page).await.unwrap();
            remaining = commands.len();

            total_count = get_failed_commands_count(&state.db)
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        4 => {
            commands = get_deleted_commands(&state.db, page).await.unwrap();
            remaining = commands.len();

            total_count = get_deleted_commands_count(&state.db)
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        5 => {
            commands = get_commands_by_status(&state.db, page, payload.filter_value.unwrap())
                .await
                .unwrap();
            remaining = commands.len();

            total_count = get_commands_count_by_status(&state.db, payload.filter_value.unwrap())
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        6 => {
            commands = get_commands_by_key(&state.db, page, payload.filter_value.unwrap())
                .await
                .unwrap();
            remaining = commands.len();

            total_count = get_commands_count_by_key(&state.db, payload.filter_value.unwrap())
                .await
                .unwrap()
                .count
                .try_into()
                .unwrap();
        }
        _ => {
            println!("Invalid filter type");
            commands = vec![];
            total_count = 0;
            remaining = 0;
        }
    };

    commands.iter().for_each(|command| {
        json_response.push_str(command.to_json().as_str());
        if remaining != 1 {
            json_response.push_str(",");
        }
        remaining -= 1;
    });

    let pages = (total_count as f64 / 25.0).ceil() as i64;
    json_response.push_str("], \"total_count\": ");
    json_response.push_str(total_count.to_string().as_str());
    json_response.push_str(", \"pages\": ");
    json_response.push_str(pages.to_string().as_str());
    json_response.push_str(", \"page\": ");
    json_response.push_str(page.to_string().as_str());
    json_response.push_str("}");

    (StatusCode::OK, json_response)
}

/// Represents the statistics of the commands in the database.
#[derive(Debug, Clone, Deserialize)]
pub struct CommandStatistics {
    /// The total number of completed commands.
    pub completed: i64,
    /// The total number of failed commands.
    pub failed: i64,
    /// The total number of in progress commands.
    pub in_progress: i64,
}

/// Fetches command statistics from the database.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// 
/// # Returns
/// - A `CommandStatistics` representing the command statistics.
/// - An `Error` if an error occurs.
pub async fn fetch_command_statistics(db: &SqlitePool) -> Result<CommandStatistics, Error> {
    let completed = get_completed_commands_count(db).await.unwrap().count;
    let failed = get_failed_commands_count(db).await.unwrap().count;
    let in_progress = get_in_progress_commands_count(db).await.unwrap().count;

    Ok(CommandStatistics {
        completed,
        failed,
        in_progress,
    })
}

/// Deletes all commands associated with a key.
/// 
/// # Arguments
/// - `db`: A reference to the database connection pool.
/// - `key`: The key of the commands to delete.
/// 
/// # Returns
/// - `Ok(())` if the commands are successfully deleted.
/// - `Err(Error)` if an error occurs.
pub async fn delete_commands_by_key(db: &SqlitePool, key: i64) -> Result<(), Error> {
    sqlx::query("DELETE FROM commands WHERE associated_key = ?")
        .bind(key)
        .execute(db)
        .await?;

    Ok(())
}
