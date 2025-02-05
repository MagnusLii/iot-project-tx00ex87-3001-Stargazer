use clap::Parser;
use webserver::{app::App, init::settings::Settings, init::setup::setup};

/// Stargazer webserver
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// Server address
    #[arg(short, long)]
    address: Option<String>,
    /// HTTP port
    #[arg(short, long)]
    port_http: Option<u16>,
    /// HTTPS port
    #[arg(short = 's', long)]
    port_https: Option<u16>,
    /// HTTP disable
    #[arg(long)]
    disable_http: bool,
    /// HTTP allowed for API (Overrides disable_http)
    #[arg(long)]
    enable_http_api: bool,
    /// HTTPS disable
    #[arg(long)]
    disable_https: bool,
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
    dbg!(&args);

    let settings = Settings::new(
        args.address,
        args.port_http,
        args.port_https,
        if args.disable_http { Some(true) } else { None },
        if args.enable_http_api {
            Some(true)
        } else {
            None
        },
        if args.disable_https { Some(true) } else { None },
        args.db_dir,
        args.assets_dir,
    )
    .unwrap_or_else(|e| panic!("Error during settings initialization: {}", e));
    settings.print();

    let http = !settings.disable_http;
    let http_api = settings.enable_http_api;
    let https = !settings.disable_https;

    if !http && !http_api && !https {
        println!("No protocols enabled. Exiting..");
        return Ok(());
    }

    let http_address = format!("{}:{}", &settings.address, settings.port_http);
    let https_address = format!("{}:{}", &settings.address, settings.port_https);
    let user_db_path = format!("{}/users.db", settings.db_dir);
    let api_db_path = format!("{}/api.db", settings.db_dir);

    if let Ok(resources) = setup(&user_db_path, &api_db_path, &settings.assets_dir).await {
        if http && https {
            tokio::spawn(alt_server(
                http_address.clone(),
                resources.user_db.clone(),
                resources.api_db.clone(),
                resources.image_dir.clone(),
                false,
            ));
        } else if !http && http_api {
            tokio::spawn(alt_server(
                http_address.clone(),
                resources.user_db.clone(),
                resources.api_db.clone(),
                resources.image_dir.clone(),
                true,
            ));
        }
        let secure = if https { true } else { false };
        App::new(resources.user_db, resources.api_db, resources.image_dir)
            .await?
            .serve(
                if secure {
                    &https_address
                } else {
                    &http_address
                },
                secure,
            )
            .await
    } else {
        panic!("Error during setup. Exiting..");
    }
}

async fn alt_server(
    address: String,
    user_db: sqlx::SqlitePool,
    api_db: sqlx::SqlitePool,
    image_dir: webserver::web::images::ImageDirectory,
    api_only: bool,
) {
    let alt_server;
    match App::new(user_db, api_db, image_dir).await {
        Ok(app) => alt_server = app,
        Err(e) => {
            println!("Alternate (HTTP) server exited with error: {}", e);
            return;
        }
    }

    if !api_only {
        match alt_server.serve(&address, false).await {
            Ok(_) => println!("Alternate (HTTP) server exited"),
            Err(e) => println!("Alternate (HTTP) server exited with error: {}", e),
        }
    } else {
        match alt_server.serve_api_only(&address, false).await {
            Ok(_) => println!("Alternate (HTTP API) server exited"),
            Err(e) => println!("Alternate (HTTP API) server exited with error: {}", e),
        }
    }
}
