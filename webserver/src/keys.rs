use crate::{err::Error, SharedState};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
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
pub struct NewKeyQuery {
    name: String,
}

pub async fn new_key(
    State(state): State<SharedState>,
    Json(name): Json<NewKeyQuery>,
) -> impl IntoResponse {
    let key = create_key(&state.db, name.name).await;

    (StatusCode::OK, Html(key))
}

#[derive(Deserialize)]
pub struct DeleteKeyQuery {
    id: i64,
}

// NOTE: Currently only allows deletion of keys that are not in use
pub async fn remove_key(
    State(state): State<SharedState>,
    Query(key): Query<DeleteKeyQuery>,
) -> impl IntoResponse {
    println!("Deleting key: {}", key.id);
    delete_key(&state.db, key.id).await;

    (StatusCode::OK, "Success\n")
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
