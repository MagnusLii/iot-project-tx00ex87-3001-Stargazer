use crate::err::Error;
use serde::{Deserialize, Serialize};
use sqlx::{FromRow, SqlitePool};

/// Represents a diagnostics table page
pub struct Diagnostics {
    pub page: u64,
    pub pages: u64,
    pub data: Vec<DiagnosticsSql>,
    pub status: u8,
}

impl std::fmt::Debug for Diagnostics {
    /// Formats the Diagnostics struct for debugging
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Diagnostics")
            .field("page", &self.page)
            .field("pages", &self.pages)
            .field("status", &self.status)
            .field("data len", &self.data.len())
            .finish()
    }
}

/// Represents a diagnostics table row from the database
#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct DiagnosticsSql {
    /// The name of the key/device related to the diagnostic
    pub name: String,
    /// The status of the diagnostic
    pub status: String,
    /// The message of the diagnostic
    pub message: String,
    /// The datetime of the diagnostic
    pub datetime: String,
}

/// Fetches diagnostics from the database
/// 
/// # Arguments
/// * `name` - The name of the key/device to filter by
/// * `page` - The page number to fetch
/// * `status` - The status of the diagnostics to filter by
/// * `db` - The database connection pool
/// 
/// # Returns
/// * A Diagnostics struct containing the diagnostics data
/// * An Error if the query fails
pub async fn get_diagnostics(
    name: Option<String>,
    page: Option<u32>,
    status: Option<u8>,
    db: &SqlitePool,
) -> Result<Diagnostics, Error> {
    dbg!(&name, &page, &status);
    let size = 25;
    let page = if let Some(p) = page {
        if p > 0 {
            p
        } else {
            1
        }
    } else {
        1
    };

    let offset = (page - 1) * size;

    let (by_name, name) = if let Some(name) = name {
        (true, name)
    } else {
        (false, "".to_string())
    };

    let (by_status, status) = if let Some(status) = status {
        (
            true,
            match status {
                1 => 1,
                2 => 2,
                3 => 3,
                _ => 0,
            },
        )
    } else {
        (false, 0)
    };

    println!(
        "Fetching diagnostics by_name: {}, name: {}, by_status: {}, status: {}",
        by_name, name, by_status, status
    );

    let result = if by_name && by_status {
        let count: u64 = sqlx::query_scalar(GET_DIAGNOSTICS_COUNT_BY_NAME_AND_STATUS)
            .bind(&name)
            .bind(status)
            .fetch_one(db)
            .await?;
        let total_pages = (count + (size as u64 - 1)) / size as u64;

        let diagnostics = sqlx::query_as(GET_DIAGNOSTICS_BY_NAME_AND_STATUS)
            .bind(name)
            .bind(status)
            .bind(size)
            .bind(offset)
            .fetch_all(db)
            .await?;

        Diagnostics {
            page: page as u64,
            pages: total_pages,
            data: diagnostics,
            status,
        }
    } else if by_name {
        let count: u64 = sqlx::query_scalar(GET_DIAGNOSTICS_COUNT_BY_NAME)
            .bind(&name)
            .fetch_one(db)
            .await?;
        let total_pages = (count + (size as u64 - 1)) / size as u64;

        let diagnostics = sqlx::query_as(GET_DIAGNOSTICS_BY_NAME)
            .bind(name)
            .bind(size)
            .bind(offset)
            .fetch_all(db)
            .await?;

        Diagnostics {
            page: page as u64,
            pages: total_pages,
            data: diagnostics,
            status: 0,
        }
    } else if by_status {
        let count: u64 = sqlx::query_scalar(GET_DIAGNOSTICS_COUNT_BY_STATUS)
            .bind(status.clone() as u8)
            .fetch_one(db)
            .await?;
        let total_pages = (count + (size as u64 - 1)) / size as u64;

        let diagnostics = sqlx::query_as(GET_DIAGNOSTICS_BY_STATUS)
            .bind(status as u8)
            .bind(size)
            .bind(offset)
            .fetch_all(db)
            .await?;

        Diagnostics {
            page: page as u64,
            pages: total_pages,
            data: diagnostics,
            status,
        }
    } else {
        let count: u64 = sqlx::query_scalar(GET_DIAGNOSTICS_COUNT)
            .fetch_one(db)
            .await?;

        let total_pages = (count + (size as u64 - 1)) / size as u64;

        let diagnostics = sqlx::query_as(GET_ALL_DIAGNOSTICS)
            .bind(size)
            .bind(offset)
            .fetch_all(db)
            .await?;

        Diagnostics {
            page: page as u64,
            pages: total_pages,
            data: diagnostics,
            status: 0,
        }
    };

    //dbg!(&result);

    Ok(result)
}

/// Represents a diagnostic status row from the database
#[derive(Debug, Deserialize, FromRow)]
pub struct DiagnosticNamesSql {
    pub name: String,
}

/// Fetches the list of valid device names from the database
/// 
/// # Arguments
/// * `db` - The database connection pool
/// 
/// # Returns
/// * A Vec of DiagnosticNamesSql containing the device names
/// * An Error if the query fails
pub async fn get_diagnostics_names(db: &SqlitePool) -> Result<Vec<DiagnosticNamesSql>, Error> {
    let names = sqlx::query_as(
        "SELECT DISTINCT keys.name AS name
        FROM diagnostics
        JOIN keys ON diagnostics.token = keys.api_token",
    )
    .fetch_all(db)
    .await?;

    Ok(names)
}

const GET_DIAGNOSTICS_BY_NAME: &str =
        "SELECT keys.name AS name, diagnostics_status.status AS status, diagnostics.message AS message, datetime(diagnostics.time, 'unixepoch') AS datetime
        FROM diagnostics
        JOIN keys ON diagnostics.token = keys.api_token
        JOIN diagnostics_status ON diagnostics.status = diagnostics_status.id
        WHERE keys.name = ?
        LIMIT ? OFFSET ?";

const GET_DIAGNOSTICS_BY_STATUS: &str =
        "SELECT keys.name AS name, diagnostics_status.status AS status, diagnostics.message AS message, datetime(diagnostics.time, 'unixepoch') AS datetime
        FROM diagnostics
        JOIN keys ON diagnostics.token = keys.api_token
        JOIN diagnostics_status ON diagnostics.status = diagnostics_status.id
        WHERE diagnostics.status = ?
        LIMIT ? OFFSET ?";

const GET_DIAGNOSTICS_BY_NAME_AND_STATUS: &str =
        "SELECT keys.name AS name, diagnostics_status.status AS status, diagnostics.message AS message, datetime(diagnostics.time, 'unixepoch') AS datetime
        FROM diagnostics
        JOIN keys ON diagnostics.token = keys.api_token
        JOIN diagnostics_status ON diagnostics.status = diagnostics_status.id
        WHERE keys.name = ? AND diagnostics.status = ?
        LIMIT ? OFFSET ?";

const GET_ALL_DIAGNOSTICS: &str =
        "SELECT keys.name AS name, diagnostics_status.status AS status, diagnostics.message AS message, datetime(diagnostics.time, 'unixepoch') AS datetime
        FROM diagnostics
        JOIN keys ON diagnostics.token = keys.api_token
        JOIN diagnostics_status ON diagnostics.status = diagnostics_status.id
        LIMIT ? OFFSET ?";

const GET_DIAGNOSTICS_COUNT: &str = "SELECT COUNT(*) FROM diagnostics";

const GET_DIAGNOSTICS_COUNT_BY_NAME: &str =
    "SELECT COUNT(*) FROM diagnostics JOIN keys ON diagnostics.token = keys.api_token WHERE keys.name = ?";

const GET_DIAGNOSTICS_COUNT_BY_STATUS: &str =
    "SELECT COUNT(*) FROM diagnostics WHERE diagnostics.status = ?";

const GET_DIAGNOSTICS_COUNT_BY_NAME_AND_STATUS: &str =
    "SELECT COUNT(*) FROM diagnostics JOIN keys ON diagnostics.token = keys.api_token WHERE keys.name = ? AND diagnostics.status = ?";

pub const STATUS_DEFAULT: &str =
    "<option value=\"\" selected>All</option><option value=\"1\">Info</option><option value=\"2\">Warning</option><option value=\"3\">Error</option>";
