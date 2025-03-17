use crate::{
    api,
    auth::{
        backend::Backend,
        login::{login, login_page, logout},
        user,
    },
    keys,
    web::{commands, images::ImageDirectory, routes},
    SharedState,
};
use axum::{
    extract::DefaultBodyLimit,
    routing::{delete, get, post},
    Router,
};
use axum_login::{
    login_required,
    tower_sessions::{cookie::Key, Expiry, MemoryStore, SessionManagerLayer},
    AuthManagerLayerBuilder,
};
use axum_messages::MessagesManagerLayer;
use axum_server::tls_rustls::RustlsConfig;
use sqlx::SqlitePool;
use std::net::TcpListener;
use time::Duration;
use tower_http::services::ServeDir;

/// The Main application object
/// Multiple instances of this struct can be created to serve different parts of the application
pub struct App {
    shared_state: SharedState,
    session_store: MemoryStore,
    key: Key,
    backend: Backend,
    assets_dir: String,
    tls_config: Option<RustlsConfig>,
}

impl App {
    /// Create a new application object
    /// 
    /// # Arguments
    /// * `user_db` - The user database connection pool
    /// * `api_db` - The API database connection pool
    /// * `image_dir` - The directory containing images
    /// * `assets_dir` - The directory containing assets
    /// * `tls_config` - The TLS configuration (if one exists)
    /// 
    /// # Returns
    /// * An `App` object
    /// * An error if the initialization fails
    pub async fn new(
        user_db: SqlitePool,
        api_db: SqlitePool,
        image_dir: ImageDirectory,
        assets_dir: String,
        tls_config: Option<RustlsConfig>,
    ) -> Result<Self, Box<dyn std::error::Error>> {
        let session_store = MemoryStore::default();
        let key = Key::generate();
        let backend = Backend::new(user_db);
        let shared_state = SharedState {
            db: api_db,
            image_dir,
        };

        Ok(Self {
            shared_state,
            session_store,
            key,
            backend,
            assets_dir,
            tls_config,
        })
    }

    /// Start a server and serve the web application and API
    /// 
    /// # Arguments
    /// * `address` - The address to bind to
    /// * `https` - Whether to use HTTPS
    /// 
    /// # Returns
    /// * `Ok(())` if the server exited successfully
    /// * An error if the server failed
    pub async fn serve(self, address: &str, https: bool) -> Result<(), Box<dyn std::error::Error>> {
        let session_layer = SessionManagerLayer::new(self.session_store)
            .with_secure(false)
            .with_expiry(Expiry::OnInactivity(Duration::days(1)))
            .with_signed(self.key);

        let auth_layer = AuthManagerLayerBuilder::new(self.backend, session_layer).build();

        let router = Router::<_>::new()
            .route("/", get(routes::root))
            .route("/gallery", get(routes::gallery))
            .route("/control", get(routes::control))
            .route("/control", post(commands::request_commands_info))
            .route("/control/keys", get(routes::api_keys))
            .route("/control/keys", post(keys::new_key))
            .route("/control/keys", delete(keys::remove_key))
            .route("/control/command", post(commands::new_command))
            .route("/control/command", delete(commands::remove_command))
            .route("/control/diagnostics", get(routes::diagnostics))
            .route("/users", get(routes::user_management))
            .route("/users", post(user::new_user))
            .route("/users", delete(user::remove_user))
            .route("/users/current", post(user::current_user))
            .route("/user", get(routes::user_page))
            .route("/user", post(user::modify_user))
            .nest_service(
                "/assets/images",
                ServeDir::new(&self.shared_state.image_dir.path),
            )
            .route_layer(login_required!(Backend, login_url = "/login"))
            .route("/login", get(login_page))
            .route("/login", post(login))
            .route("/logout", get(logout))
            .layer(MessagesManagerLayer)
            .layer(auth_layer)
            .nest_service("/assets", ServeDir::new(&self.assets_dir))
            .route(
                "/api/upload",
                post(api::upload::upload_image).layer(DefaultBodyLimit::max(262_144_000)),
            )
            .route("/api/command", get(api::commands::fetch_command))
            .route("/api/command", post(api::commands::respond_command))
            .route("/api/diagnostics", post(api::diagnostics::send_diagnostics))
            .route("/api/time", get(api::time_srv::time))
            .with_state(self.shared_state)
            .route("/favicon.ico", get(routes::favicon))
            .fallback(routes::unknown_route);

        if !https {
            let listener = TcpListener::bind(address)?;

            println!("Listening on: http://{}", address);

            axum_server::from_tcp(listener)
                .serve(router.into_make_service())
                .await?;
        } else {
            let tls_config = if let Some(tls_config) = self.tls_config {
                tls_config
            } else {
                return Err("TLS config missing".into());
            };

            let listener = TcpListener::bind(address)?;

            println!("Listening on: https://{}", address);

            axum_server::from_tcp_rustls(listener, tls_config)
                .serve(router.into_make_service())
                .await?;
        }

        Ok(())
    }

    /// Start a server and serve the API only
    /// 
    /// # Arguments
    /// * `address` - The address to bind to
    /// * `https` - Whether to use HTTPS
    /// 
    /// # Returns
    /// * `Ok(())` if the server exited successfully
    /// * An error if the server failed
    pub async fn serve_api_only(
        self,
        address: &str,
        https: bool,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let router = Router::<_>::new()
            .route(
                "/api/upload",
                post(api::upload::upload_image).layer(DefaultBodyLimit::max(262_144_000)),
            )
            .route("/api/command", get(api::commands::fetch_command))
            .route("/api/command", post(api::commands::respond_command))
            .route("/api/diagnostics", post(api::diagnostics::send_diagnostics))
            .route("/api/time", get(api::time_srv::time))
            .with_state(self.shared_state);

        if !https {
            let listener = TcpListener::bind(address)?;

            println!("Listening on: http://{} (API Only)", address);

            axum_server::from_tcp(listener)
                .serve(router.into_make_service())
                .await?;
        } else {
            let tls_config = if let Some(tls_config) = self.tls_config {
                tls_config
            } else {
                return Err("TLS config missing".into());
            };

            let listener = TcpListener::bind(address)?;

            println!("Listening on: https://{} (API Only)", address);

            axum_server::from_tcp_rustls(listener, tls_config)
                .serve(router.into_make_service())
                .await?;
        }

        Ok(())
    }
}
