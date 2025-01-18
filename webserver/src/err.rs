use tokio::task;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error(transparent)]
    Sqlx(#[from] sqlx::Error),

    #[error(transparent)]
    TaskJoin(#[from] task::JoinError),

    #[error(transparent)]
    Password(#[from] PasswordError),
}

#[cfg(feature = "pw_hash")]
#[derive(Debug, thiserror::Error)]
pub enum PasswordError {
    #[error("argon2 hashing error")]
    Argon2(argon2::Error),

    #[error(transparent)]
    PasswordHash(#[from] password_hash::Error),

    #[error("password verification failed")]
    Other,
}

#[cfg(not(feature = "pw_hash"))]
#[derive(Debug, thiserror::Error)]
pub enum PasswordError {
    #[error("password verification failed")]
    Other,
}
