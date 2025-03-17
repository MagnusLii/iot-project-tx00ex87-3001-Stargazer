use crate::{
    api, auth,
    init::settings::Mode,
    web::images::{self, ImageDirectory},
};
use axum_server::tls_rustls::RustlsConfig;
use sqlx::SqlitePool;
use std::{env, path::Path};
use tokio::{fs, io};

/// Represents the application resources returned by the setup function.
pub struct Resources {
    /// The user database connection pool.
    pub user_db: SqlitePool,
    /// The API database connection pool.
    pub api_db: SqlitePool,
    /// The image directory.
    pub image_dir: ImageDirectory,
    /// The assets directory path.
    pub assets_dir: String,
    /// The TLS configuration, if enabled.
    pub tls_config: Option<RustlsConfig>,
    /// The primary server details.
    pub primary_server: ServerDetails,
    /// The secondary server details, if applicable.
    pub secondary_server: Option<ServerDetails>,
}

/// Main setup function for the webserver to ensure everything is correct and
/// in the places where they should be.
///
/// # Arguments
///
/// * `user_db_path`: Path to the user database.
/// * `api_db_path`: Path to the API database.
/// * `assets_dir_path`: Path to the assets directory.
/// * `certs_dir_path`: Path to the certificates directory.
/// * `tls`: Whether TLS is enabled.
/// * `address`: The server address.
/// * `http_port`: The HTTP port.
/// * `https_port`: The HTTPS port.
/// * `mode`: The application mode.
///
/// # Returns
///
/// A `Result` containing the `Resources` instance or an error.
pub async fn setup(
    user_db_path: &str,
    api_db_path: &str,
    assets_dir_path: &str,
    certs_dir_path: &str,
    tls: bool,
    address: &str,
    http_port: u16,
    https_port: u16,
    mode: Mode,
) -> Result<Resources, Box<dyn std::error::Error>> {
    let user_db: SqlitePool;
    let api_db: SqlitePool;
    let image_dir: ImageDirectory;
    let assets_dir: String;
    let tls_config: Option<RustlsConfig>;

    match setup_user_database(user_db_path).await {
        Ok(db) => user_db = db,
        Err(e) => panic!("Error setting up user database: {}", e),
    };

    match setup_api_database(api_db_path).await {
        Ok(db) => api_db = db,
        Err(e) => panic!("Error setting up API database: {}", e),
    };

    match setup_cache_dir("stargazer-ws").await {
        Ok(_) => {}
        Err(e) => panic!("Error setting up tmp dir: {}", e),
    };

    match setup_file_dirs(assets_dir_path).await {
        Ok(_) => {
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

            assets_dir = String::from(assets_dir_path);
        }
        Err(e) => panic!("Error setting up file dirs: {}", e),
    };

    if tls {
        tls_config = match setup_tls_config(certs_dir_path).await {
            Ok(tls_config) => Some(tls_config),
            Err(e) => panic!("Error setting up TLS config: {}", e),
        };
    } else {
        tls_config = None;
    }

    let (primary_server, secondary_server) =
        match setup_server_details(address, http_port, https_port, mode).await {
            Ok((primary, secondary)) => (primary, secondary),
            Err(e) => return Err(e),
        };

    println!("Setup complete.");
    Ok(Resources {
        user_db,
        api_db,
        image_dir,
        assets_dir,
        tls_config,
        primary_server,
        secondary_server,
    })
}

/// Sets up the user database if it doesn't exist.
///
/// # Arguments
///
/// * `user_db_path`: Path to the user database.
///
/// # Returns
///
/// A `Result` containing the `SqlitePool` or a `sqlx::Error`
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

/// Sets up the API database if it doesn't exist.
///
/// # Arguments
///
/// * `api_db_path`: Path to the API database.
///
/// # Returns
///
/// A `Result` containing the `SqlitePool` or a `sqlx::Error`.
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
        api::setup::create_diagnostics_status_table(&api_db).await;
        api::setup::create_image_table(&api_db).await;
        api::setup::create_objects_table(&api_db).await;
        api::setup::create_position_table(&api_db).await;
    }

    Ok(api_db)
}

/// Sets up the cache directory.
///
/// # Arguments
///
/// * `tmp_dir`: The name of the temporary directory.
///
/// # Returns
///
/// Returns a `Result` indicating success or an `io::Error`.
pub async fn setup_cache_dir(tmp_dir: &str) -> Result<(), io::Error> {
    let cache_dir_path = env::temp_dir().join(tmp_dir);
    fs::create_dir_all(&cache_dir_path).await?;
    Ok(())
}

/// Sets up the file directories and populates them if necessary, 
/// This includes assets, css, js and images.
///
/// # Arguments
///
/// * `assets_dir_path`: Path to the assets directory.
///
/// # Returns
///
/// A `Result` indicating success or an `io::Error`.
pub async fn setup_file_dirs(assets_dir_path: &str) -> Result<(), io::Error> {
    let assets_path = Path::new(assets_dir_path);

    if let Ok(_) = fs::create_dir_all(assets_path).await {
        if !assets_path.join("main.css").exists() {
            let main_css = std::include_str!("../../css/main.css");
            fs::write(assets_path.join("main.css"), main_css).await?;
        }
        if !assets_path.join("favicon.ico").exists() {
            let favicon = std::include_bytes!("../../icons/favicon.ico");
            fs::write(assets_path.join("favicon.ico"), favicon).await?;
        }

        fs::create_dir_all(assets_path.join("images")).await?;

        match fs::create_dir_all(assets_path.join("js")).await {
            Ok(_) => {
                if !assets_path.join("js/commands.js").exists() {
                    let commands_js = std::include_str!("../../js/commands.js");
                    fs::write(assets_path.join("js/commands.js"), commands_js).await?;
                }
                if !assets_path.join("js/diagnostics.js").exists() {
                    let diagnostics_js = std::include_str!("../../js/diagnostics.js");
                    fs::write(assets_path.join("js/diagnostics.js"), diagnostics_js).await?;
                }
                if !assets_path.join("js/images.js").exists() {
                    let images_js = std::include_str!("../../js/images.js");
                    fs::write(assets_path.join("js/images.js"), images_js).await?;
                }
                if !assets_path.join("js/keys.js").exists() {
                    let keys_js = std::include_str!("../../js/keys.js");
                    fs::write(assets_path.join("js/keys.js"), keys_js).await?;
                }
                if !assets_path.join("js/sitewide.js").exists() {
                    let sitewide_js = std::include_str!("../../js/sitewide.js");
                    fs::write(assets_path.join("js/sitewide.js"), sitewide_js).await?;
                }
                if !assets_path.join("js/users.js").exists() {
                    let users_js = std::include_str!("../../js/users.js");
                    fs::write(assets_path.join("js/users.js"), users_js).await?;
                }
            }
            Err(e) => eprintln!("Error creating assets/js directory: {}", e),
        }
    }

    Ok(())
}

/// Sets up the TLS configuration.
///
/// # Arguments
///
/// * `certs_dir_path`: Path to the certificates directory.
///
/// # Returns
///
/// A `Result` containing the `RustlsConfig` or an `io::Error`.
/// 
/// # Panics
///
/// Panics if the server certificate or key is not found.
pub async fn setup_tls_config(certs_dir_path: &str) -> Result<RustlsConfig, io::Error> {
    let crt_path = Path::new(certs_dir_path).join("server.crt");
    match crt_path.try_exists() {
        Ok(exists) => {
            if !exists {
                panic!(
                    "Server certificate (server.crt) not found in directory: {}",
                    certs_dir_path
                )
            }
        }
        Err(e) => panic!("Error checking for server certificate: {}", e),
    };

    let key_path = Path::new(certs_dir_path).join("server.key");
    match key_path.try_exists() {
        Ok(exists) => {
            if !exists {
                panic!(
                    "Server key (server.key) not found in directory: {}",
                    certs_dir_path
                )
            }
        }
        Err(e) => panic!("Error checking for server key: {}", e),
    };

    let tls_config = RustlsConfig::from_pem_file(&crt_path, &key_path).await?;
    let cs = tls_config
        .get_inner()
        .crypto_provider()
        .cipher_suites
        .clone();
    println!("Cipher suites: {:#?}", cs);

    Ok(tls_config)
}

/// Represents the individual server details.
pub struct ServerDetails {
    /// The server address.
    pub address: String,
    /// Whether the server is using HTTPS
    pub secure: bool,
    /// Whether the server is API only.
    pub api_only: bool,
}

/// Sets up the server details based on the mode.
/// 
/// # Arguments
/// 
/// * `address`: The server address.
/// * `http_port`: The HTTP port.
/// * `https_port`: The HTTPS port.
/// * `mode`: The application mode.
///
/// # Returns
/// 
/// A `Result` containing the primary and secondary `ServerDetails` or an error.
async fn setup_server_details(
    address: &str,
    http_port: u16,
    https_port: u16,
    mode: Mode,
) -> Result<(ServerDetails, Option<ServerDetails>), Box<dyn std::error::Error>> {
    let http_address = format!("{}:{}", address, http_port);
    let https_address = format!("{}:{}", address, https_port);

    match mode {
        Mode::Both => Ok((
            ServerDetails {
                address: https_address,
                secure: true,
                api_only: false,
            },
            Some(ServerDetails {
                address: http_address,
                secure: false,
                api_only: false,
            }),
        )),
        Mode::BothApi => Ok((
            ServerDetails {
                address: https_address,
                secure: true,
                api_only: true,
            },
            Some(ServerDetails {
                address: http_address,
                secure: false,
                api_only: true,
            }),
        )),
        Mode::HttpPlusHttpsApi => Ok((
            ServerDetails {
                address: http_address,
                secure: false,
                api_only: false,
            },
            Some(ServerDetails {
                address: https_address,
                secure: true,
                api_only: true,
            }),
        )),
        Mode::HttpsPlusHttpApi => Ok((
            ServerDetails {
                address: https_address,
                secure: true,
                api_only: false,
            },
            Some(ServerDetails {
                address: http_address,
                secure: false,
                api_only: true,
            }),
        )),
        Mode::Http => Ok((
            ServerDetails {
                address: http_address,
                secure: false,
                api_only: false,
            },
            None,
        )),
        Mode::HttpApi => Ok((
            ServerDetails {
                address: http_address,
                secure: false,
                api_only: true,
            },
            None,
        )),
        Mode::Https => Ok((
            ServerDetails {
                address: https_address,
                secure: true,
                api_only: false,
            },
            None,
        )),
        Mode::HttpsApi => Ok((
            ServerDetails {
                address: https_address,
                secure: true,
                api_only: true,
            },
            None,
        )),
        _ => Err("Invalid mode".into()),
    }
}
