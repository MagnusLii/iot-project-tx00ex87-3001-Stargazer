use sqlx::SqlitePool;
use std::env;

use webserver::{api::ApiState, app::App, setup::setup};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let address = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:7878".to_string());

    let user_db_path = "db/users.db";
    let api_db_path = "db/api.db";

    let user_db: SqlitePool;
    let api_state: ApiState;

    if let Ok(dbs) = setup(user_db_path, api_db_path).await {
        user_db = dbs.user_db;
        api_state = dbs.api_state;
    } else {
        panic!("Error during setup. Exiting..");
    }

    App::new(user_db, api_state).await?.serve(&address).await
}
