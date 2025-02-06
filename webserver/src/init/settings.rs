use config::{Config, ConfigError, File};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct Settings {
    pub address: String,
    pub port_http: u16,
    pub port_https: u16,
    pub disable_http: bool,
    pub enable_http_api: bool,
    pub disable_https: bool,
    pub db_dir: String,
    pub assets_dir: String,
    pub certs_dir: String,
}

impl Settings {
    pub fn new(
        address: Option<String>,
        port_http: Option<u16>,
        port_https: Option<u16>,
        disable_http: Option<bool>,
        enable_http_api: Option<bool>, // Only takes effect if disable_http is true
        disable_https: Option<bool>,
        db_dir: Option<String>,
        assets_dir: Option<String>,
        certs_dir: Option<String>,
    ) -> Result<Self, ConfigError> {
        let builder = Config::builder()
            .set_default("address", "127.0.0.1")?
            .set_default("port_http", 8080)?
            .set_default("port_https", 8443)?
            .set_default("disable_http", false)?
            .set_default("enable_http_api", false)?
            .set_default("disable_https", false)?
            .set_default("db_dir", "db")?
            .set_default("assets_dir", "assets")?
            .set_default("certs_dir", "certs")?
            .add_source(File::with_name("config").required(false))
            .set_override_option("address", address)?
            .set_override_option("port_http", port_http)?
            .set_override_option("port_https", port_https)?
            .set_override_option("disable_http", disable_http)?
            .set_override_option("enable_http_api", enable_http_api)?
            .set_override_option("disable_https", disable_https)?
            .set_override_option("db_dir", db_dir)?
            .set_override_option("assets_dir", assets_dir)?
            .set_override_option("certs_dir", certs_dir)?;

        let config = builder.build()?;

        Ok(Settings {
            address: config.get("address")?,
            port_http: config.get("port_http")?,
            port_https: config.get("port_https")?,
            disable_http: config.get("disable_http")?,
            enable_http_api: if config.get("disable_http")? {
                config.get("enable_http_api")?
            } else {
                true
            },
            disable_https: config.get("disable_https")?,
            db_dir: config.get("db_dir")?,
            assets_dir: config.get("assets_dir")?,
            certs_dir: config.get("certs_dir")?,
        })
    }

    pub fn print(&self) {
        println!("Address: {}", self.address);
        if !self.disable_http {
            println!("HTTP Enabled");
            println!("HTTP Port: {}", self.port_http);
        } else if self.enable_http_api {
            println!("HTTP API Only Enabled");
            println!("HTTP Port: {}", self.port_http);
        }
        if !self.disable_https {
            println!("HTTPS Enabled");
            println!("HTTPS Port: {}", self.port_https);
        }
        println!("DB Dir: {}", self.db_dir);
        println!("Assets Dir: {}", self.assets_dir);
        println!("Certs Dir: {}", self.certs_dir);
    }
}
