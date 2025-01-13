use crate::{
    api, auth,
    web::images::{self, ImageDirectory},
};
use sqlx::SqlitePool;
use std::{env, path::Path};
use tokio::{fs, io};

pub struct Resources {
    pub user_db: SqlitePool,
    pub api_db: SqlitePool,
    pub image_dir: ImageDirectory,
}

pub async fn setup(
    user_db_path: &str,
    api_db_path: &str,
    assets_dir_path: &str,
) -> Result<Resources, Box<dyn std::error::Error>> {
    let user_db: SqlitePool;
    let api_db: SqlitePool;
    let image_dir: ImageDirectory;

    match setup_user_database(user_db_path).await {
        Ok(db) => user_db = db,
        Err(e) => panic!("Error setting up user database: {}", e),
    };

    match setup_api_database(api_db_path).await {
        Ok(db) => api_db = db,
        Err(e) => panic!("Error setting up API database: {}", e),
    };

    match setup_cache_dir("sgwebserver").await {
        Ok(_) => {}
        Err(e) => panic!("Error setting up tmp dir: {}", e),
    };

    match setup_file_dirs(assets_dir_path).await {
        Ok(_) => {
            /* TODO: Create a function for this */
            image_dir = ImageDirectory::new(
                Path::new(assets_dir_path).join("images"),
                vec![
                    String::from("jpg"),
                    String::from("png"),
                    String::from("jpeg"),
                ],
            );
            images::check_images(&api_db, &image_dir, true)
                .await
                .unwrap();
            /* TODO: Create a function for this */
        }
        Err(e) => panic!("Error setting up file dirs: {}", e),
    };

    Ok(Resources {
        user_db,
        api_db,
        image_dir,
    })
}

pub async fn setup_user_database(user_db_path: &str) -> Result<SqlitePool, sqlx::Error> {
    let path = Path::new(user_db_path);
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
    let user_db = SqlitePool::connect_with(options_users).await.unwrap();

    if !exists {
        auth::setup::create_user_table(&user_db).await;
        auth::setup::create_admin(&user_db).await;
    }

    Ok(user_db) // Return the user_db
}

pub async fn setup_api_database(api_db_path: &str) -> Result<SqlitePool, sqlx::Error> {
    let path = Path::new(api_db_path);
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
    let api_db = SqlitePool::connect_with(options_api).await.unwrap();

    if !exists {
        api::setup::create_api_keys_table(&api_db).await;
        api::setup::create_command_table(&api_db).await;
        api::setup::create_diagnostics_table(&api_db).await;
        api::setup::create_image_table(&api_db).await;
        api::setup::create_objects_table(&api_db).await;
        api::setup::create_position_table(&api_db).await;
    }

    Ok(api_db)
}

// TODO: Reevaluate need for this
pub async fn setup_cache_dir(tmp_dir: &str) -> Result<(), io::Error> {
    let cache_dir_path = env::temp_dir().join(tmp_dir);
    fs::create_dir_all(&cache_dir_path).await?;
    Ok(())
}

// Creates the assets and images directories if they don't exist
// This is mostly for purposes of preventing the server from crashing if the directories are missing
// but at least the assets directory should be there and populated before the server is started
pub async fn setup_file_dirs(assets_dir_path: &str) -> Result<(), io::Error> {
    let assets_path = Path::new(assets_dir_path);

    fs::create_dir_all(assets_path.join("images")).await?;
    Ok(())
}
