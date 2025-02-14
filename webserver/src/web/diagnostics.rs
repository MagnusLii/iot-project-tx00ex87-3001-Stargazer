use crate::err::Error;
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct DiagnosticsSql {
    pub name: String,
    pub status: String,
    pub message: String,
    pub datetime: String,
}

pub async fn get_diagnostics(
    name: Option<String>,
    page: Option<u32>,
    db: &SqlitePool,
) -> Result<Vec<DiagnosticsSql>, Error> {
    let size = 25;
    let page = page.unwrap_or(1);
    let offset = (page - 1) * size;
    println!("Page: {}, Offset: {}", page, offset);

    if let Some(name) = name {
        let diagnostics = sqlx::query_as(
            "SELECT keys.name AS name, diagnostics_status.status AS status, diagnostics.message AS message, datetime(diagnostics.time, 'unixepoch') AS datetime 
            FROM diagnostics 
            JOIN keys ON diagnostics.token = keys.api_token
            JOIN diagnostics_status ON diagnostics.status = diagnostics_status.id
            WHERE name = ?
            LIMIT ? OFFSET ?",
        )
        .bind(name)
        .bind(size)
        .bind(offset)
        .fetch_all(db)
        .await?;

        Ok(diagnostics)
    } else {
        let diagnostics = sqlx::query_as(
            "SELECT keys.name AS name, diagnostics_status.status AS status, diagnostics.message AS message, datetime(diagnostics.time, 'unixepoch') AS datetime
            FROM diagnostics
            JOIN keys ON diagnostics.token = keys.api_token
            JOIN diagnostics_status ON diagnostics.status = diagnostics_status.id
            LIMIT ? OFFSET ?",
        )
        .bind(size)
        .bind(offset)
        .fetch_all(db)
        .await?;

        Ok(diagnostics)
    }
}
