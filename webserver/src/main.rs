use sqlx::SqlitePool;
use webserver::{api::ApiState, app::App, settings::Settings, setup::setup};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let settings = Settings::new(None, None, None, None).unwrap();

    let address = format!("{}:{}", settings.address, settings.port);
    let user_db_path = format!("{}/users.db", settings.db_dir);
    let api_db_path = format!("{}/api.db", settings.db_dir);

    let user_db: SqlitePool;
    let api_state: ApiState;

    if let Ok(dbs) = setup(&user_db_path, &api_db_path).await {
        user_db = dbs.user_db;
        api_state = dbs.api_state;
    } else {
        panic!("Error during setup. Exiting..");
    }

    App::new(user_db, api_state).await?.serve(&address).await
}
