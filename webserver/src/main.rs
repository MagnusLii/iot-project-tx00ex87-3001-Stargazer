use axum_server::tls_rustls::RustlsConfig;
use clap::Parser;
use sqlx::SqlitePool;
use webserver::{
    app::App, init::settings::Settings, init::setup::setup, web::images::ImageDirectory,
};

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
    /// Certificate directory
    #[arg(long)]
    certs_dir: Option<String>,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

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
        args.certs_dir,
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

    if let Ok(resources) = setup(
        &user_db_path,
        &api_db_path,
        &settings.assets_dir,
        &settings.certs_dir,
        https,
    )
    .await
    {
        let (pri_address, sec_address, pri_api_only, sec_api_only, pri_secure) = if http && https {
            (https_address, Some(http_address), false, false, true)
        } else if !http && https {
            if http_api {
                (https_address, Some(http_address), false, true, true)
            } else {
                (https_address, None, false, false, true)
            }
        } else if http && !https {
            (http_address, None, false, false, false)
        } else {
            (http_address, None, true, false, false)
        };

        if let Some(sec_address) = sec_address {
            let sec_server = tokio::spawn(server(
                sec_address,
                resources.user_db.clone(),
                resources.api_db.clone(),
                resources.image_dir.clone(),
                resources.tls_config.clone(),
                sec_api_only,
                !pri_secure,
            ));
            let pri_server = tokio::spawn(server(
                pri_address,
                resources.user_db,
                resources.api_db,
                resources.image_dir,
                resources.tls_config,
                pri_api_only,
                pri_secure,
            ));

            let (pri, sec) = tokio::join!(pri_server, sec_server);

            if let Err(e) = pri {
                println!("Primary server exited with error: {}", e);
            }

            if let Err(e) = sec {
                println!("Secondary server exited with error: {}", e);
            }
        } else {
            let srv = server(
                pri_address,
                resources.user_db,
                resources.api_db,
                resources.image_dir,
                resources.tls_config,
                pri_api_only,
                pri_secure,
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

async fn server(
    address: String,
    user_db: SqlitePool,
    api_db: SqlitePool,
    image_dir: ImageDirectory,
    tls_config: Option<RustlsConfig>,
    api_only: bool,
    https: bool,
) -> Result<(), Box<dyn std::error::Error + Send>> {
    let server: App;
    match App::new(user_db, api_db, image_dir, tls_config).await {
        Ok(app) => server = app,
        Err(e) => {
            println!("Server exited before serving with error: {}", e);
            return Ok(());
        }
    }

    if !api_only {
        match server.serve(&address, https).await {
            Ok(_) => println!("Server exited"),
            Err(e) => println!("Server exited with error: {}", e),
        }
    } else {
        match server.serve_api_only(&address, https).await {
            Ok(_) => println!("Server exited"),
            Err(e) => println!("Server exited with error: {}", e),
        }
    }

    Ok(())
}
