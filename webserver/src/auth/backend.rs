use crate::{auth::password, auth::user::User, err::Error};
use async_trait::async_trait;
use axum_login::{AuthnBackend, UserId};
use serde::Deserialize;
use sqlx::SqlitePool;
use tokio::task;

/// Represents user authentication credentials.
#[derive(Debug, Clone, Deserialize)]
pub struct Credentials {
    /// The username of the user.
    pub username: String,
    /// The password of the user.
    pub password: String,
    /// An optional redirect URL after login.
    pub next: Option<String>,
}

/// Authentication backend managing user operations.
#[derive(Debug, Clone)]
pub struct Backend {
    /// Database connection pool.
    db: SqlitePool,
}

impl Backend {
    /// Creates a new authentication backend.
    /// 
    /// # Arguments
    /// * `db` - The database connection pool.
    pub fn new(db: SqlitePool) -> Self {
        Self { db }
    }

    /// Adds a new user to the database.
    /// 
    /// # Arguments
    /// * `user` - The user's credentials.
    /// 
    /// # Returns
    /// * `Result<(), Error>` - An empty result if successful, or an error.
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

    /// Deletes a user from the database by their ID.
    /// 
    /// # Arguments
    /// * `id` - The user's unique ID.
    /// 
    /// # Returns
    /// * `Result<(), Error>` - An empty result if successful, or an error.
    pub async fn delete_user(&self, id: i64) -> Result<(), Error> {
        sqlx::query("DELETE FROM users WHERE id = ?")
            .bind(id)
            .execute(&self.db)
            .await?;

        Ok(())
    }

    /// Retrieves all users from the database, optionally filtering by privilege level.
    /// 
    /// # Arguments
    /// * `privileged` - Optional filter for privileged users.
    /// 
    /// # Returns
    /// * `Result<Vec<User>, Error>` - A list of users or an error.
    pub async fn get_users(&self, privileged: Option<bool>) -> Result<Vec<User>, Error> {
        let users: Vec<User>;

        if let Some(privileged) = privileged {
            users = sqlx::query_as("SELECT * FROM users WHERE superuser = ?")
                .bind(privileged)
                .fetch_all(&self.db)
                .await?;
        } else {
            users = sqlx::query_as("SELECT * FROM users")
                .fetch_all(&self.db)
                .await?;
        }

        Ok(users)
    }

    /// Changes the username of an existing user.
    /// 
    /// # Arguments
    /// * `id` - The user's unique ID.
    /// * `username` - The new username.
    /// 
    /// # Returns
    /// * `Result<(), Error>` - An empty result if successful, or an error.
    pub async fn change_username(&self, id: i64, username: String) -> Result<(), Error> {
        sqlx::query("UPDATE users SET username = ? WHERE id = ?")
            .bind(username)
            .bind(id)
            .execute(&self.db)
            .await?;

        Ok(())
    }

    /// Changes the password of an existing user.
    /// 
    /// # Arguments
    /// * `id` - The user's unique ID.
    /// * `password` - The new password.
    /// 
    /// # Returns
    /// * `Result<(), Error>` - An empty result if successful, or an error.
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

    /// Authenticates a user based on their credentials.
    /// 
    /// # Arguments
    /// * `creds` - The credentials provided by the user.
    /// 
    /// # Returns
    /// * `Result<Option<User>, Error>` - The authenticated user if successful, otherwise `None` or an error.
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

    /// Retrieves a user from the database by their ID.
    /// 
    /// # Arguments
    /// * `user_id` - The unique identifier of the user.
    /// 
    /// # Returns
    /// * `Result<Option<User>, Error>` - The user if found, otherwise `None` or an error.
    async fn get_user(&self, user_id: &UserId<Self>) -> Result<Option<Self::User>, Self::Error> {
        let user = sqlx::query_as("SELECT * FROM users WHERE id = ?")
            .bind(user_id)
            .fetch_optional(&self.db)
            .await?;

        Ok(user)
    }
}
