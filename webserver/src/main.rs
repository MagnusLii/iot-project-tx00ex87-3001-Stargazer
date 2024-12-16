use clap::Parser;
use webserver::{app::App, init::settings::Settings, init::setup::setup};

/// Stargazer webserver
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// Server address
    #[arg(short, long)]
    address: Option<String>,
    /// Server port
    #[arg(short, long)]
    port: Option<u16>,
    /// Database directory
    #[arg(long)]
    db_dir: Option<String>,
    /// Assets directory
    #[arg(long)]
    assets_dir: Option<String>,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    let settings = Settings::new(args.address, args.port, args.db_dir, args.assets_dir).unwrap();

    let address = format!("{}:{}", settings.address, settings.port);
    let user_db_path = format!("{}/users.db", settings.db_dir);
    let api_db_path = format!("{}/api.db", settings.db_dir);

    if let Ok(dbs) = setup(&user_db_path, &api_db_path, &settings.assets_dir).await {
        App::new(dbs.user_db, dbs.api_state)
            .await?
            .serve(&address)
            .await
    } else {
        panic!("Error during setup. Exiting..");
    }
}
