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

impl MultipleCommandSql {
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

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct CommandCount {
    pub count: i64,
    pub status: i64,
}

pub async fn get_all_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands")
        .fetch_one(db)
        .await?;

    Ok(count)
}

pub async fn get_completed_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE status = 3")
        .fetch_one(db)
        .await?;

    Ok(count)
}

pub async fn get_failed_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as(
        "SELECT COUNT(*) AS count, status FROM commands WHERE status BETWEEN -5 AND -1",
    )
    .fetch_one(db)
    .await?;

    Ok(count)
}

pub async fn get_in_progress_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as(
        "SELECT COUNT(*) AS count, status FROM commands WHERE status BETWEEN 0 AND 2",
    )
    .fetch_one(db)
    .await?;

    Ok(count)
}

pub async fn get_deleted_commands_count(db: &SqlitePool) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE status = -6")
        .fetch_one(db)
        .await?;

    Ok(count)
}

pub async fn get_commands_count_by_status(
    db: &SqlitePool,
    status: i64,
) -> Result<CommandCount, Error> {
    let count = sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE status = ?")
        .bind(status)
        .fetch_one(db)
        .await?;

    Ok(count)
}

pub async fn get_commands_count_by_key(db: &SqlitePool, key: i64) -> Result<CommandCount, Error> {
    let count =
        sqlx::query_as("SELECT COUNT(*) AS count, status FROM commands WHERE associated_key = ?")
            .bind(key)
            .fetch_one(db)
            .await?;

    Ok(count)
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

#[derive(Debug, Clone, Deserialize)]
pub struct RequestCommandsJson {
    pub filter_type: i64, /* 0 = all, 1 = completed, 2 = in progress, 3 = failed, 4 = deleted, 5 =
                           * specific status, 6 = specific key (id)
                           */
    pub filter_value: Option<i64>,
    pub page: Option<i64>,
}

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

#[derive(Debug, Clone, Deserialize)]
pub struct CommandStatistics {
    pub completed: i64,
    pub failed: i64,
    pub in_progress: i64,
}

pub async fn fetch_command_statistics(db: &SqlitePool) -> CommandStatistics {
    let completed = get_completed_commands_count(db).await.unwrap().count;
    let failed = get_failed_commands_count(db).await.unwrap().count;
    let in_progress = get_in_progress_commands_count(db).await.unwrap().count;

    CommandStatistics {
        completed,
        failed,
        in_progress,
    }
}
