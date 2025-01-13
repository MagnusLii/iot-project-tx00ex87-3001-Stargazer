use crate::err::Error;
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct DiagnosticsSql {
    pub name: String,
    pub status: String,
    pub message: String,
}

pub async fn get_diagnostics(
    name: Option<String>,
    db: &SqlitePool,
) -> Result<Vec<DiagnosticsSql>, Error> {
    if let Some(name) = name {
        let diagnostics = sqlx::query_as(
            "SELECT keys.name AS name, diagnostics.status AS status, diagnostics.message AS message 
            FROM diagnostics JOIN keys ON diagnostics.token = keys.api_token WHERE name = ?"
        )
        .bind(name)
        .fetch_all(db)
        .await?;
        Ok(diagnostics)
    } else {
        let diagnostics = sqlx::query_as(
            "SELECT keys.name AS name, diagnostics.status AS status, diagnostics.message AS message
            FROM diagnostics JOIN keys ON diagnostics.token = keys.api_token",
        )
        .fetch_all(db)
        .await?;
        Ok(diagnostics)
    }
}
