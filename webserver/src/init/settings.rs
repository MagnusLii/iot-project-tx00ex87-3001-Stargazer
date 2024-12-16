use config::{Config, ConfigError, File};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct Settings {
    pub address: String,
    pub port: u16,
    pub db_dir: String,
    pub assets_dir: String,
}

impl Settings {
    pub fn new(
        address: Option<String>,
        port: Option<u16>,
        db_dir: Option<String>,
        assets_dir: Option<String>,
    ) -> Result<Self, ConfigError> {
        let builder = Config::builder()
            .set_default("address", "127.0.0.1")?
            .set_default("port", 8080)?
            .set_default("db_dir", "db")?
            .set_default("assets_dir", "assets")?
            .add_source(File::with_name("config").required(false))
            .set_override_option("address", address)?
            .set_override_option("port", port)?
            .set_override_option("db_dir", db_dir)?
            .set_override_option("assets_dir", assets_dir)?;

        let config = builder.build()?;

        Ok(Settings {
            address: config.get("address")?,
            port: config.get("port")?,
            db_dir: config.get("db_dir")?,
            assets_dir: config.get("assets_dir")?,
        })
    }

    pub fn print(&self) {
        println!("Address: {}", self.address);
        println!("Port: {}", self.port);
        println!("DB Dir: {}", self.db_dir);
        println!("Assets Dir: {}", self.assets_dir);
    }
}
