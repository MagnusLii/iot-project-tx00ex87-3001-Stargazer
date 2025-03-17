use crate::{err::Error, web::commands::delete_commands_by_key, SharedState};
use axum::{
    extract::{rejection::JsonRejection, Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

/// Represents an API key stored in the database
#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct Key {
    /// The ID of the key
    pub id: i64,
    /// The API token
    pub api_token: String,
    /// The name of the key
    pub name: String,
}

/// Get all keys from the database
/// 
/// # Arguments
/// * `db` - The database connection pool
/// 
/// # Returns
/// * A vector of `Key` objects
/// * An `Error` if the query fails
pub async fn get_keys(db: &SqlitePool) -> Result<Vec<Key>, Error> {
    let keys: Vec<Key> = sqlx::query_as("SELECT * FROM keys").fetch_all(db).await?;

    Ok(keys)
}

/// Create a new key in the database
/// 
/// # Arguments
/// * `db` - The database connection pool
/// * `name` - The name of the key
/// 
/// # Returns
/// The API token of the new key
async fn create_key(db: &SqlitePool, name: String) -> String {
    let api_key = uuid::Uuid::new_v4().to_string();
    sqlx::query("INSERT INTO keys (api_token, name) VALUES (?, ?)")
        .bind(&api_key)
        .bind(name)
        .execute(db)
        .await
        .unwrap();
    api_key
}

/// Delete a key from the database
/// 
/// # Arguments
/// * `db` - The database connection pool
/// * `id` - The ID of the key to delete
async fn delete_key(db: &SqlitePool, id: i64) {
    sqlx::query("DELETE FROM keys WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

/// Name of the key to create wrapped in a JSON payload
#[derive(Deserialize)]
pub struct NewKeyJson {
    /// The name of the key
    name: String,
}

/// The route handler for creating a new key
/// 
/// # Arguments
/// * `state` - The shared state of the application
/// * `payload` - The JSON payload containing the name of the key
/// 
/// # Returns
/// * A HTTP response containing the API token of the new key
pub async fn new_key(
    State(state): State<SharedState>,
    payload: Result<Json<NewKeyJson>, JsonRejection>,
) -> impl IntoResponse {
    match payload {
        Ok(Json(json)) => {
            if json.name.is_empty() {
                return (StatusCode::BAD_REQUEST, "Error: missing name".to_string());
            }

            let key = create_key(&state.db, json.name).await;
            (StatusCode::OK, key)
        }
        Err(e) => {
            eprintln!("Error: {}", e);
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                "Error: issue parsing request".to_string(),
            )
        }
    }
}

/// Query parameters for deleting a key
#[derive(Deserialize)]
pub struct DeleteKeyQuery {
    /// The ID of the key to delete
    id: i64,
}

/// The route handler for deleting a key
/// 
/// # Arguments
/// * `state` - The shared state of the application
/// * `key` - The query parameters containing the ID of the key to delete
/// 
/// # Returns
/// A HTTP response indicating success or failure
pub async fn remove_key(
    State(state): State<SharedState>,
    Query(key): Query<DeleteKeyQuery>,
) -> impl IntoResponse {
    println!("Deleting key and associated commands: {}", key.id);
    match delete_commands_by_key(&state.db, key.id).await {
        Ok(_) => {
            delete_key(&state.db, key.id).await;
        }
        Err(e) => {
            eprintln!("Error deleting commands: {}", e);
            return (
                StatusCode::INTERNAL_SERVER_ERROR,
                "Error deleting key and its commands",
            );
        }
    }
    (StatusCode::OK, "Success")
}

/// Verify that a key is valid
/// 
/// # Arguments
/// * `token` - The API token to verify
/// * `db` - The database connection pool
/// 
/// # Returns
/// * `true` if the key is valid
/// * `false` if the key is invalid
pub async fn verify_key(token: &str, db: &SqlitePool) -> bool {
    let keys = get_keys(db).await.unwrap();
    for key in keys {
        if key.api_token == token {
            return true;
        }
    }
    false
}
