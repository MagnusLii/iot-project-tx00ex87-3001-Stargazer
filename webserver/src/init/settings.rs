use config::{Config, ConfigError, File};
use std::net::{Ipv4Addr, Ipv6Addr};

/// Represents the application settings.
#[derive(Debug)]
pub struct Settings {
    /// The address to bind to.
    pub address: String,
    /// The HTTP port.
    pub port_http: u16,
    /// The HTTPS port.
    pub port_https: u16,
    /// Enables HTTP.
    pub enable_http: bool,
    /// Enables HTTP API only mode.
    pub enable_http_api: bool,
    /// Enables HTTPS.
    pub enable_https: bool,
    /// Enables HTTPS API only mode.
    pub enable_https_api: bool,
    /// The directory for the database.
    pub db_dir: String,
    /// The directory for assets.
    pub assets_dir: String,
    /// The directory for TLS certificates.
    pub certs_dir: String,
}

impl Settings {
    /// Creates a new `Settings` instance by loading configuration from a file and applying overrides.
    ///
    /// # Arguments
    ///
    /// * `config_file`: Optional path to the configuration file.
    /// * `working_dir`: Optional working directory.
    /// * `address`: Optional override for the address.
    /// * `port_http`: Optional override for the HTTP port.
    /// * `port_https`: Optional override for the HTTPS port.
    /// * `enable_http`: Optional override for enabling HTTP.
    /// * `enable_http_api`: Optional override for enabling HTTP API only mode.
    /// * `enable_https`: Optional override for enabling HTTPS.
    /// * `enable_https_api`: Optional override for enabling HTTPS API only mode.
    /// * `db_dir`: Optional override for the database directory.
    /// * `assets_dir`: Optional override for the assets directory.
    /// * `certs_dir`: Optional override for the certificates directory.
    ///
    /// # Returns
    ///
    /// Returns a `Result` containing the `Settings` instance or a `ConfigError`.
    pub fn new(
        config_file: Option<String>,
        working_dir: Option<String>,
        address: Option<String>,
        port_http: Option<u16>,
        port_https: Option<u16>,
        enable_http: Option<bool>,
        enable_http_api: Option<bool>, // Only takes effect if http is disabled
        enable_https: Option<bool>,
        enable_https_api: Option<bool>, // Only takes effect if https is disabled
        db_dir: Option<String>,
        assets_dir: Option<String>,
        certs_dir: Option<String>,
    ) -> Result<Self, ConfigError> {
        // Working directory
        let wd = match working_dir {
            Some(dir) => dir,
            None => match std::env::current_dir() {
                Ok(dir) => dir.display().to_string(),
                Err(err) => {
                    eprintln!("Failed to get current working directory: {}", err);
                    ".".to_string()
                }
            },
        };
        println!("Working directory: {}", wd);

        // Default locations ($WORKING_DIR/)
        let config_file_default = format!("{}/config", wd);
        let db_dir_default = format!("{}/db", wd);
        let assets_dir_default = format!("{}/assets", wd);
        let certs_dir_default = format!("{}/certs", wd);

        let config_file = match config_file {
            Some(config_file_path) => match std::fs::exists(&config_file_path) {
                Ok(true) => config_file_path,
                _ => {
                    eprintln!("Specified config file not found: {}", config_file_path);
                    config_file_default
                }
            },
            None => config_file_default,
        };

        let builder = Config::builder()
            .set_default("address", "127.0.0.1")?
            .set_default("port_http", 8080)?
            .set_default("port_https", 8443)?
            .set_default("enable_http", true)?
            .set_default("enable_http_api", false)?
            .set_default("enable_https", false)?
            .set_default("enable_https_api", false)?
            .set_default("db_dir", db_dir_default)?
            .set_default("assets_dir", assets_dir_default)?
            .set_default("certs_dir", certs_dir_default)?
            .add_source(File::with_name(&config_file).required(false))
            .set_override_option("address", address)?
            .set_override_option("port_http", port_http)?
            .set_override_option("port_https", port_https)?
            .set_override_option("enable_http", enable_http)?
            .set_override_option("enable_http_api", enable_http_api)?
            .set_override_option("enable_https", enable_https)?
            .set_override_option("enable_https_api", enable_https_api)?
            .set_override_option("db_dir", db_dir)?
            .set_override_option("assets_dir", assets_dir)?
            .set_override_option("certs_dir", certs_dir)?;

        let config = builder.build()?;

        let conf_address: String = config.get("address")?;

        if let Err(_) = conf_address.parse::<Ipv4Addr>() {
            if let Err(_) = conf_address.parse::<Ipv6Addr>() {
                return Err(ConfigError::Message(format!(
                    "Parsed an invalid address {}",
                    conf_address
                )));
            }
        }

        Ok(Settings {
            address: config.get("address")?,
            port_http: config.get("port_http")?,
            port_https: config.get("port_https")?,
            enable_http: config.get("enable_http")?,
            enable_http_api: config.get("enable_http_api")?,
            enable_https: config.get("enable_https")?,
            enable_https_api: config.get("enable_https_api")?,
            db_dir: config.get("db_dir")?,
            assets_dir: config.get("assets_dir")?,
            certs_dir: config.get("certs_dir")?,
        })
    }

    /// Prints the loaded settings to the console.
    pub fn print(&self) {
        println!("Address: {}", self.address);
        if self.enable_http {
            println!("HTTP Enabled");
            println!("HTTP Port: {}", self.port_http);
        } else if self.enable_http_api {
            println!("HTTP API Only Enabled");
            println!("HTTP Port: {}", self.port_http);
        } else {
            println!("HTTP Disabled");
        }
        if self.enable_https {
            println!("HTTPS Enabled");
            println!("HTTPS Port: {}", self.port_https);
        } else if self.enable_https_api {
            println!("HTTPS API Only Enabled");
            println!("HTTPS Port: {}", self.port_https);
        } else {
            println!("HTTPS Disabled");
        }
        println!("DB Dir: {}", self.db_dir);
        println!("Assets Dir: {}", self.assets_dir);
        println!("Certs Dir: {}", self.certs_dir);
    }

    /// Determines the application's operating mode based on the loaded settings.
    ///
    /// # Returns
    ///
    /// * `Mode` enum representing the application mode based on the settings.
    pub fn get_mode(&self) -> Mode {
        match (
            self.enable_http,
            self.enable_http_api,
            self.enable_https,
            self.enable_https_api,
        ) {
            (true, false | true, false, false) => Mode::Http,
            (false, false, true, false | true) => Mode::Https,
            (false, true, false, false) => Mode::HttpApi,
            (false, false, false, true) => Mode::HttpsApi,
            (true, false | true, false, true) => Mode::HttpPlusHttpsApi,
            (false, true, true, false | true) => Mode::HttpsPlusHttpApi,
            (true, false | true, true, false | true) => Mode::Both,
            (false, true, false, true) => Mode::BothApi,
            _ => Mode::None,
        }
    }
}

/// Represents the application mode.
#[derive(Debug, Clone)]
pub enum Mode {
    None,
    Http,
    HttpApi,
    Https,
    HttpsApi,
    HttpPlusHttpsApi,
    HttpsPlusHttpApi,
    Both,
    BothApi,
}

impl From<u8> for Mode {
    /// Converts a `u8` value to a `Mode` enum.
    fn from(mode: u8) -> Self {
        match mode {
            0 => Mode::None,
            1 => Mode::Http,
            2 => Mode::HttpApi,
            3 => Mode::Https,
            4 => Mode::HttpsApi,
            5 => Mode::HttpPlusHttpsApi,
            6 => Mode::HttpsPlusHttpApi,
            7 => Mode::Both,
            8 => Mode::BothApi,
            _ => Mode::None,
        }
    }
}
