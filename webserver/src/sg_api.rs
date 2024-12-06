use crate::sg_err::Error;
use axum::{
    extract::{Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
};
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct ApiKey {
    pub id: i64,
    pub api_key: String,
}

/*
pub struct ApiBackend {
    db: SqlitePool,
}
impl ApiBackend {
    pub fn new(db: SqlitePool) -> Self {
        Self { db }
    }

    pub async fn get_api_keys(&self) -> Result<Vec<ApiKey>, Error> {
        let api_keys: Vec<ApiKey> = sqlx::query_as("SELECT * FROM api_keys")
            .fetch_all(&self.db)
            .await?;

        Ok(api_keys)
    }

    pub async fn create_api_key(db: &SqlitePool) -> String {
        let api_key = uuid::Uuid::new_v4().to_string();
        sqlx::query("INSERT INTO api_keys (api_key) VALUES (?)")
            .bind(&api_key)
            .execute(db)
            .await
            .unwrap();
        api_key
    }

    pub async fn delete_api_key(db: &SqlitePool, id: i64) {
        sqlx::query("DELETE FROM api_keys WHERE id = ?")
            .bind(id)
            .execute(db)
            .await
            .unwrap();
    }
}
*/
#[derive(Clone)]
pub struct ApiState {
    pub db: SqlitePool,
}

/*
pub async fn api_routes() -> Router<ApiState> {
    let state = ApiState {
        db: SqlitePool::connect("sqlite:db/api.db").await.unwrap(),
    };

    Router::new()
        .route("/control/keys", get(api_keys))
        .with_state(state)
}
*/
pub async fn create_api_keys_table(db: &SqlitePool) {
    sqlx::query(
        "CREATE TABLE IF NOT EXISTS api_keys (
            id INTEGER PRIMARY KEY,
            api_key TEXT NOT NULL UNIQUE
        )",
    )
    .execute(db)
    .await
    .unwrap();
}

pub async fn create_command_table(db: &SqlitePool) {}

pub async fn get_api_keys(db: &SqlitePool) -> Result<Vec<ApiKey>, Error> {
    let api_keys: Vec<ApiKey> = sqlx::query_as("SELECT * FROM api_keys")
        .fetch_all(db)
        .await?;

    Ok(api_keys)
}

pub async fn api_keys(State(state): State<ApiState>) -> impl IntoResponse {
    let mut html = include_str!("../html/keys.html").to_string();
    let html_keys = get_api_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| {
            format!(
                "<li>{}</li><button onclick=\"deleteKey({})\">Delete</button>",
                key.api_key, key.id
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    (StatusCode::OK, Html(html))
}

pub async fn create_api_key(db: &SqlitePool) -> String {
    let api_key = uuid::Uuid::new_v4().to_string();
    sqlx::query("INSERT INTO api_keys (api_key) VALUES (?)")
        .bind(&api_key)
        .execute(db)
        .await
        .unwrap();
    api_key
}

pub async fn delete_api_key(db: &SqlitePool, id: i64) {
    sqlx::query("DELETE FROM api_keys WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

pub async fn new_key(State(state): State<ApiState>) -> impl IntoResponse {
    let key = create_api_key(&state.db).await;

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
