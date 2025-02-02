use crate::{auth::password, auth::user::User, err::Error};
use async_trait::async_trait;
use axum_login::{AuthnBackend, UserId};
use serde::Deserialize;
use sqlx::SqlitePool;
use tokio::task;

#[derive(Debug, Clone, Deserialize)]
pub struct Credentials {
    pub username: String,
    pub password: String,
    pub next: Option<String>,
}

#[derive(Debug, Clone)]
pub struct Backend {
    db: SqlitePool,
}

impl Backend {
    pub fn new(db: SqlitePool) -> Self {
        Self { db }
    }

    pub async fn add_user(&self, user: Credentials) -> Result<(), Error> {
        #[cfg(feature = "pw_hash")]
        let pw = password::generate_phc_string(&user.password)?;

        #[cfg(not(feature = "pw_hash"))]
        let pw = user.password;

        sqlx::query("INSERT INTO users (username, password) VALUES (?, ?)")
            .bind(&user.username)
            .bind(&pw)
            .execute(&self.db)
            .await?;

        Ok(())
    }

    pub async fn delete_user(&self, id: i64) -> Result<(), Error> {
        sqlx::query("DELETE FROM users WHERE id = ?")
            .bind(id)
            .execute(&self.db)
            .await?;

        Ok(())
    }

    pub async fn get_users(&self) -> Result<Vec<User>, Error> {
        let users = sqlx::query_as("SELECT * FROM users")
            .fetch_all(&self.db)
            .await?;

        Ok(users)
    }

    pub async fn change_username(&self, id: i64, username: String) -> Result<(), Error> {
        sqlx::query("UPDATE users SET username = ? WHERE id = ?")
            .bind(username)
            .bind(id)
            .execute(&self.db)
            .await?;

        Ok(())
    }

    pub async fn change_password(&self, id: i64, password: String) -> Result<(), Error> {
        #[cfg(feature = "pw_hash")]
        let pw = password::generate_phc_string(&password)?;

        #[cfg(not(feature = "pw_hash"))]
        let pw = password;

        sqlx::query("UPDATE users SET password = ? WHERE id = ?")
            .bind(pw)
            .bind(id)
            .execute(&self.db)
            .await?;

        Ok(())
    }
}

#[async_trait]
impl AuthnBackend for Backend {
    type User = User;
    type Credentials = Credentials;
    type Error = Error;

    async fn authenticate(
        &self,
        creds: Self::Credentials,
    ) -> Result<Option<Self::User>, Self::Error> {
        let user: Option<Self::User> = sqlx::query_as("SELECT * FROM users WHERE username = ? ")
            .bind(creds.username)
            .fetch_optional(&self.db)
            .await?;

        task::spawn_blocking(move || {
            Ok(user.filter(|u| password::verify_password(&creds.password, &u.password).is_ok()))
        })
        .await?
    }

    async fn get_user(&self, user_id: &UserId<Self>) -> Result<Option<Self::User>, Self::Error> {
        let user = sqlx::query_as("SELECT * FROM users WHERE id = ?")
            .bind(user_id)
            .fetch_optional(&self.db)
            .await?;

        Ok(user)
    }
}
