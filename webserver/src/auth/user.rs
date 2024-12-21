use axum_login::AuthUser;
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

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
