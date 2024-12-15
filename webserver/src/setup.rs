use crate::{api, auth, images};
use sqlx::SqlitePool;
use std::env;
use tokio::{fs, io};

pub struct Databases {
    pub user_db: SqlitePool,
    pub api_state: api::ApiState,
}

pub async fn setup(
    user_db_path: &str,
    api_db_path: &str,
) -> Result<Databases, Box<dyn std::error::Error>> {
    let user_db: SqlitePool;
    let api_state: api::ApiState;

    match setup_user_database(user_db_path).await {
        Ok(db) => user_db = db,
        Err(e) => panic!("Error setting up user database: {}", e),
    };

    match setup_api_database(api_db_path).await {
        Ok(db) => api_state = db,
        Err(e) => panic!("Error setting up API database: {}", e),
    };

    match setup_cache_dir("sgwebserver").await {
        Ok(_) => {}
        Err(e) => panic!("Error setting up tmp dir: {}", e),
    };

    images::update_gallery().await;

    Ok(Databases { user_db, api_state })
}

pub async fn setup_user_database(user_db_path: &str) -> Result<sqlx::SqlitePool, sqlx::Error> {
    let path = std::path::Path::new(user_db_path);
    let exists = path.exists();

    if !exists {
        println!("User database does not exist. Creating...");
        fs::create_dir_all(path.parent().unwrap())
            .await
            .expect("Failed to create directory");
    }

    let options_users = sqlx::sqlite::SqliteConnectOptions::new()
        .filename(user_db_path)
        .create_if_missing(true);
    let user_db = sqlx::SqlitePool::connect_with(options_users).await.unwrap();

    if !exists {
        auth::setup::create_user_table(&user_db).await;
        auth::setup::create_admin(&user_db).await;
    }

    Ok(user_db) // Return the user_db
}

pub async fn setup_api_database(api_db_path: &str) -> Result<api::ApiState, sqlx::Error> {
    let path = std::path::Path::new(api_db_path);
    let exists = path.exists();

    if !exists {
        println!("API database does not exist. Creating...");
        fs::create_dir_all(path.parent().unwrap())
            .await
            .expect("Failed to create directory");
    }

    let options_api = sqlx::sqlite::SqliteConnectOptions::new()
        .filename(api_db_path)
        .create_if_missing(true);
    let api_db = sqlx::SqlitePool::connect_with(options_api).await.unwrap();

    if !exists {
        api::setup::create_api_keys_table(&api_db).await;
        api::setup::create_command_table(&api_db).await;
        api::setup::create_diagnostics_table(&api_db).await;
    }

    Ok(api::ApiState { db: api_db }) // Return the api_db in an ApiState struct
}

pub async fn setup_cache_dir(tmp_dir: &str) -> Result<(), io::Error> {
    let cache_dir_path = env::temp_dir().join(tmp_dir);
    fs::create_dir_all(&cache_dir_path).await?;
    Ok(())
}
