use crate::{err::Error, web::commands::delete_commands_by_key, SharedState};
use axum::{
    extract::{rejection::JsonRejection, Json, Query, State},
    http::StatusCode,
    response::IntoResponse,
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct Key {
    pub id: i64,
    pub api_token: String,
    pub name: String,
}

pub async fn get_keys(db: &SqlitePool) -> Result<Vec<Key>, Error> {
    let keys: Vec<Key> = sqlx::query_as("SELECT * FROM keys").fetch_all(db).await?;

    Ok(keys)
}

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

async fn delete_key(db: &SqlitePool, id: i64) {
    sqlx::query("DELETE FROM keys WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

#[derive(Deserialize)]
pub struct NewKeyJson {
    name: String,
}

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

#[derive(Deserialize)]
pub struct DeleteKeyQuery {
    id: i64,
}

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

pub async fn verify_key(token: &str, db: &SqlitePool) -> bool {
    let keys = get_keys(db).await.unwrap();
    for key in keys {
        if key.api_token == token {
            return true;
        }
    }
    false
}
