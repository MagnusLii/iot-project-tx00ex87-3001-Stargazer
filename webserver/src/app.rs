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
use sqlx::SqlitePool;
use time::Duration;
use tokio::net::TcpListener;
use tower_http::services::ServeDir;

pub struct App {
    shared_state: SharedState,
    session_store: MemoryStore,
    key: Key,
    backend: Backend,
}

impl App {
    pub async fn new(
        user_db: SqlitePool,
        api_db: SqlitePool,
        image_dir: ImageDirectory,
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
        })
    }

    pub async fn serve(self, address: &str) -> Result<(), Box<dyn std::error::Error>> {
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
            .nest_service("/assets/images", ServeDir::new("assets/images"))
            .route_layer(login_required!(Backend, login_url = "/login"))
            .route("/login", get(login_page))
            .route("/login", post(login))
            .route("/logout", get(logout))
            .route("/test", get(crate::web::routes::test))
            .route("/test", post(commands::request_commands_info))
            .layer(MessagesManagerLayer)
            .layer(auth_layer)
            .route(
                "/api/upload",
                post(api::upload::upload_image).layer(DefaultBodyLimit::max(262_144_000)),
            )
            .route("/api/command", get(api::commands::fetch_command))
            .route("/api/command", post(api::commands::respond_command))
            .route("/api/diagnostics", post(api::diagnostics::send_diagnostics))
            .route("/api/time", get(api::time_srv::time))
            .with_state(self.shared_state)
            .nest_service("/assets", ServeDir::new("assets"))
            .fallback(routes::unknown_route);

        let listener = TcpListener::bind(address).await.unwrap();

        println!("Listening on: http://{}", address);

        axum::serve(listener, router.into_make_service()).await?;

        Ok(())
    }
}
