use axum_server::tls_rustls::RustlsConfig;
use clap::Parser;
use sqlx::SqlitePool;
use stargazer_ws::{
    app::App,
    init::{settings::Settings, setup::setup},
    web::images::ImageDirectory,
};

/// Stargazer webserver
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// Set server address
    #[arg(short, long)]
    address: Option<String>,
    /// Set HTTP port
    #[arg(short, long)]
    port_http: Option<u16>,
    /// Set HTTPS port
    #[arg(short = 's', long)]
    port_https: Option<u16>,
    /// Disable HTTP
    #[arg(long, conflicts_with = "enable_http")]
    disable_http: bool,
    /// Enable HTTP
    #[arg(long)]
    enable_http: bool,
    /// Enable HTTP API if HTTP is disabled
    #[arg(long)]
    enable_http_api: bool,
    /// Disable HTTPS
    #[arg(long, conflicts_with = "enable_https")]
    disable_https: bool,
    /// Enable HTTPS
    #[arg(long)]
    enable_https: bool,
    /// Enable HTTPS API if HTTPS is disabled
    #[arg(long)]
    enable_https_api: bool,
    /// Set config file to use (default: config.toml)
    #[arg(short, long)]
    config_file: Option<String>,
    /// Set working directory
    #[arg(short, long)]
    working_dir: Option<String>,
    /// Set database directory
    #[arg(long)]
    db_dir: Option<String>,
    /// Set assets directory
    #[arg(long)]
    assets_dir: Option<String>,
    /// Set certificate directory
    #[arg(long)]
    certs_dir: Option<String>,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    let settings = Settings::new(
        args.config_file,
        args.working_dir,
        args.address,
        args.port_http,
        args.port_https,
        if args.disable_http {
            Some(false)
        } else if args.enable_http {
            Some(true)
        } else {
            None
        },
        if args.enable_http_api {
            Some(true)
        } else {
            None
        },
        if args.disable_https {
            Some(false)
        } else if args.enable_https {
            Some(true)
        } else {
            None
        },
        if args.enable_https_api {
            Some(true)
        } else {
            None
        },
        args.db_dir,
        args.assets_dir,
        args.certs_dir,
    )?;
    settings.print();

    let http = settings.enable_http;
    let http_api = settings.enable_http_api;
    let https = settings.enable_https;
    let https_api = settings.enable_https_api;

    if !http && !http_api && !https && !https_api {
        println!("Nothing is enabled. Check your config file. Exiting...");
        return Ok(());
    }

    let user_db_path = format!("{}/users.db", settings.db_dir);
    let api_db_path = format!("{}/api.db", settings.db_dir);

    if let Ok(resources) = setup(
        &user_db_path,
        &api_db_path,
        &settings.assets_dir,
        &settings.certs_dir,
        https || https_api,
        &settings.address,
        settings.port_http,
        settings.port_https,
        settings.get_mode(),
    )
    .await
    {
        //let _ = webserver::web::images::check_images(&resources.api_db, &resources.image_dir, true)
        //    .await;

        if let Some(sec) = resources.secondary_server {
            let sec_server = tokio::spawn(server(
                sec.address,
                resources.user_db.clone(),
                resources.api_db.clone(),
                resources.image_dir.clone(),
                resources.assets_dir.clone(),
                resources.tls_config.clone(),
                sec.api_only,
                sec.secure,
            ));
            let pri_server = tokio::spawn(server(
                resources.primary_server.address,
                resources.user_db,
                resources.api_db,
                resources.image_dir,
                resources.assets_dir,
                resources.tls_config,
                resources.primary_server.api_only,
                resources.primary_server.secure,
            ));

            let (pri, sec) = tokio::join!(pri_server, sec_server);

            if let Err(e) = pri? {
                println!("Primary server exited with: {}", e);
            } else {
                println!("Primary server exited");
            }

            if let Err(e) = sec? {
                println!("Secondary server exited with: {}", e);
            } else {
                println!("Secondary server exited");
            }
        } else {
            let srv = server(
                resources.primary_server.address,
                resources.user_db,
                resources.api_db,
                resources.image_dir,
                resources.assets_dir,
                resources.tls_config,
                resources.primary_server.api_only,
                resources.primary_server.secure,
            )
            .await;

            if let Err(e) = srv {
                println!("Server exited with error: {}", e);
            }
        }

        Ok(())
    } else {
        panic!("Error during setup. Exiting..");
    }
}

/// Starts a server on the given address with selected options
/// 
/// # Arguments
/// * `address` - The address to bind to
/// * `user_db` - The user database connection pool
/// * `api_db` - The API database connection pool
/// * `image_dir` - The directory containing images
/// * `assets_dir` - The directory containing assets
/// * `tls_config` - The TLS configuration (if one exists)
/// * `api_only` - Whether to only serve the API
/// * `https` - Whether to use HTTPS
/// 
/// # Returns
/// * `Ok(())` if the server exited successfully
/// * `Err(&'static str)` if there was an error
async fn server(
    address: String,
    user_db: SqlitePool,
    api_db: SqlitePool,
    image_dir: ImageDirectory,
    assets_dir: String,
    tls_config: Option<RustlsConfig>,
    api_only: bool,
    https: bool,
) -> Result<(), &'static str> {
    let server: App;
    match App::new(user_db, api_db, image_dir, assets_dir, tls_config).await {
        Ok(app) => server = app,
        Err(e) => {
            println!("Error during app initialization: {}", e);
            return Err("Initialization error");
        }
    }

    if !api_only {
        match server.serve(&address, https).await {
            Ok(_) => println!("Server exited"),
            Err(e) => {
                println!("Error while serving: {}", e);
                return Err("Serve error");
            }
        }
    } else {
        match server.serve_api_only(&address, https).await {
            Ok(_) => println!("Server exited"),
            Err(e) => {
                println!("Error while serving: {}", e);
                return Err("Serve error");
            }
        }
    }

    Ok(())
}
