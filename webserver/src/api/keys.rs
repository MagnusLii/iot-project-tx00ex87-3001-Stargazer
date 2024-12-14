use crate::{api::ApiState, err::Error};
use axum::{
    extract::{Json, Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct ApiKey {
    pub id: i64,
    pub api_key: String,
    pub name: String,
}

pub async fn get_api_keys(db: &SqlitePool) -> Result<Vec<ApiKey>, Error> {
    let api_keys: Vec<ApiKey> = sqlx::query_as("SELECT * FROM api_keys")
        .fetch_all(db)
        .await?;

    Ok(api_keys)
}

async fn create_api_key(db: &SqlitePool, name: String) -> String {
    let api_key = uuid::Uuid::new_v4().to_string();
    sqlx::query("INSERT INTO api_keys (api_key, name) VALUES (?, ?)")
        .bind(&api_key)
        .bind(name)
        .execute(db)
        .await
        .unwrap();
    api_key
}

async fn delete_api_key(db: &SqlitePool, id: i64) {
    sqlx::query("DELETE FROM api_keys WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

#[derive(Deserialize)]
pub struct NewKey {
    name: String,
}

pub async fn new_key(State(state): State<ApiState>, Json(name): Json<NewKey>) -> impl IntoResponse {
    let key = create_api_key(&state.db, name.name).await;

    (StatusCode::OK, Html(key))
}

#[derive(Deserialize)]
pub struct DeleteKey {
    id: i64,
}

pub async fn delete_key(
    State(state): State<ApiState>,
    Query(delete_key): Query<DeleteKey>,
) -> impl IntoResponse {
    println!("Deleting key: {}", delete_key.id);
    delete_api_key(&state.db, delete_key.id).await;

    (StatusCode::OK, "Success\n")
}

pub async fn verify_key(key: &str, db: &SqlitePool) -> bool {
    let api_keys = get_api_keys(db).await.unwrap();
    for api_key in api_keys {
        if api_key.api_key == key {
            return true;
        }
    }
    false
}
