use sqlx::{sqlite::SqliteConnectOptions, SqlitePool};
use std::{env, fs};
use tokio::net::TcpListener;

use webserver::{api, auth, images, routes};

#[tokio::main]
async fn main() {
    let address = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:7878".to_string());

    // Does database exist?
    let first_run = !std::path::Path::new("db/users.db").exists();
    let token_first_run = !std::path::Path::new("db/api.db").exists();

    let options_users = SqliteConnectOptions::new()
        .filename("db/users.db")
        .create_if_missing(true);
    let user_db = SqlitePool::connect_with(options_users).await.unwrap();
    // Create tables and admin user if first run
    if first_run {
        println!("First run detected. Creating tables and admin user");
        auth::setup::create_user_table(&user_db).await;
        auth::setup::create_admin(&user_db).await;
    }

    let options_api = SqliteConnectOptions::new()
        .filename("db/api.db")
        .create_if_missing(true);
    let api_state = api::ApiState {
        db: SqlitePool::connect_with(options_api).await.unwrap(),
    };
    // Create tables and admin user if first run
    if token_first_run {
        println!("First run detected. Creating api keys table");
        api::setup::create_api_keys_table(&api_state.db).await;
        api::setup::create_command_table(&api_state.db).await;
        api::setup::create_diagnostics_table(&api_state.db).await;
    }

    let app = routes::configure(user_db).with_state(api_state);

    // Create tmp dir for caching
    let tmp = env::temp_dir();
    match fs::create_dir_all(&tmp.join("sgwebserver")) {
        Ok(_) => println!("Created tmp dir"),
        Err(e) => panic!("Error creating tmp dir: {}", e),
    };

    // Update images before serving
    images::update_gallery().await;

    let listener = TcpListener::bind(&address).await.unwrap();

    println!("Listening on: http://{}", address);

    axum::serve(listener, app).await.unwrap();
}
