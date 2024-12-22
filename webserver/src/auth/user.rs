use crate::auth::{backend::Credentials, login::AuthSession};
use axum::{
    extract::{Json, Query},
    http::StatusCode,
    response::IntoResponse,
};
use axum_login::AuthUser;
use serde::{Deserialize, Serialize};
use sqlx::FromRow;

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct User {
    pub id: i64,
    pub username: String,
    pub password: String,
}

impl AuthUser for User {
    type Id = i64;

    fn id(&self) -> Self::Id {
        self.id
    }

    fn session_auth_hash(&self) -> &[u8] {
        self.password.as_bytes()
    }
}

pub async fn current_user(auth_session: AuthSession) -> impl IntoResponse {
    let name: String;
    if auth_session.user.is_some() {
        name = auth_session.user.unwrap().username;
    } else {
        name = "unknown".to_string()
    }
    (StatusCode::OK, name)
}

pub async fn new_user(auth_session: AuthSession, user: Json<Credentials>) -> impl IntoResponse {
    println!("Attempting to add user: {}", user.username);
    if let Ok(_) = auth_session.backend.add_user(user.0).await {
        (StatusCode::OK, "User added").into_response()
    } else {
        (StatusCode::INTERNAL_SERVER_ERROR, "Error adding user").into_response()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemoveUserQuery {
    pub id: i64,
}

#[axum::debug_handler]
pub async fn remove_user(
    auth_session: AuthSession,
    user: Query<RemoveUserQuery>,
) -> impl IntoResponse {
    println!("Attempting to remove user: {}", user.id);
    if auth_session.user.unwrap().id == user.id {
        (StatusCode::FORBIDDEN, "Cannot remove yourself").into_response()
    } else {
        if let Ok(_) = auth_session.backend.delete_user(user.id).await {
            (StatusCode::OK, "User removed").into_response()
        } else {
            (StatusCode::INTERNAL_SERVER_ERROR, "Error removing user").into_response()
        }
    }
}
/*
pub async fn add_user(db: &SqlitePool, user: &User) {
    sqlx::query("INSERT INTO users (username, password) VALUES (?, ?)")
        .bind(&user.username)
        .bind(&user.password)
        .execute(db)
        .await
        .unwrap();
}

pub async fn remove_user(db: &SqlitePool, id: i64) {
    sqlx::query("DELETE FROM users WHERE id = ?")
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

pub async fn change_password(db: &SqlitePool, id: i64, password: &str) {
    sqlx::query("UPDATE users SET password = ? WHERE id = ?")
        .bind(password)
        .bind(id)
        .execute(db)
        .await
        .unwrap();
}

pub async fn get_users(db: &SqlitePool) -> Result<Vec<User>, Error> {
    let users = sqlx::query_as("SELECT id, username FROM users")
        .fetch_all(db)
        .await?;

    Ok(users)
}
*/
