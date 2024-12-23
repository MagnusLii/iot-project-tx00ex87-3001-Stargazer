use crate::{
    api::{
        commands::fetch_command, diagnostics::send_diagnostics, time_srv, upload::upload_image,
        ApiState,
    },
    auth::{
        backend::Backend,
        login::{login, login_page, logout},
        user::{current_user, modify_user, new_user, remove_user},
    },
    keys::{new_key, remove_key},
    web::{
        commands::new_command,
        routes::{
            api_keys, control, diagnostics, gallery, root, unknown_route, user_management,
            user_page,
        },
    },
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
use sqlx::SqlitePool;
use time::Duration;
use tokio::net::TcpListener;
use tower_http::services::ServeDir;

pub struct App {
    api_state: ApiState,
    session_store: MemoryStore,
    key: Key,
    backend: Backend,
}

impl App {
    pub async fn new(
        user_db: SqlitePool,
        api_state: ApiState,
    ) -> Result<Self, Box<dyn std::error::Error>> {
        let session_store = MemoryStore::default();
        let key = Key::generate();
        let backend = Backend::new(user_db);

        Ok(Self {
            api_state,
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
            .route("/", get(root))
            .route("/gallery", get(gallery))
            .route("/control", get(control))
            .route("/control/keys", get(api_keys))
            .route("/control/keys", post(new_key))
            .route("/control/keys", delete(remove_key))
            .route("/control/command", post(new_command))
            .route("/control/diagnostics", get(diagnostics))
            .route("/users", get(user_management))
            .route("/users", post(new_user))
            .route("/users", delete(remove_user))
            .route("/users/current", post(current_user))
            .route("/user", get(user_page))
            .route("/user", post(modify_user))
            .nest_service("/assets/images", ServeDir::new("assets/images"))
            .route_layer(login_required!(Backend, login_url = "/login"))
            .route("/login", get(login_page))
            .route("/login", post(login))
            .route("/logout", get(logout))
            //.route("/test", get(test))
            .layer(auth_layer)
            .route(
                "/api/upload",
                post(upload_image).layer(DefaultBodyLimit::max(262_144_000)),
            )
            .route("/api/command", get(fetch_command))
            .route("/api/diagnostics", post(send_diagnostics))
            .route("/api/time", get(time_srv::time))
            .with_state(self.api_state)
            .nest_service("/assets", ServeDir::new("assets"))
            .fallback(unknown_route);

        let listener = TcpListener::bind(address).await.unwrap();

        println!("Listening on: http://{}", address);

        axum::serve(listener, router.into_make_service()).await?;

        Ok(())
    }
}
