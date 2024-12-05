use sqlx::{sqlite::SqliteConnectOptions, SqlitePool};
use std::{env, fs};
use tokio::net::TcpListener;

mod routes;
mod sg_api;
mod sg_auth;
mod sg_err;

#[tokio::main]
async fn main() {
    let address = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:7878".to_string());

    // Does database exist?
    let first_run = !std::path::Path::new("db/users.db").exists();
    let token_first_run = !std::path::Path::new("db/api.db").exists();

    let options = SqliteConnectOptions::new()
        .filename("db/users.db")
        .create_if_missing(true);
    let user_db = SqlitePool::connect_with(options).await.unwrap();
    // Create tables and admin user if first run
    if first_run {
        println!("First run detected. Creating tables and admin user");
        sg_auth::create_user_table(&user_db).await;
        sg_auth::create_admin(&user_db).await;
    }

    let options_api = SqliteConnectOptions::new()
        .filename("db/api.db")
        .create_if_missing(true);
    let api_state = sg_api::ApiState {
        db: SqlitePool::connect_with(options_api).await.unwrap(),
    };
    // Create tables and admin user if first run
    if token_first_run {
        println!("First run detected. Creating api keys table");
        sg_api::create_api_keys_table(&api_state.db).await;
    }

    // let session_store = MemoryStore::default();
    // let session_layer = SessionManagerLayer::new(session_store);
    // let backend = sg_auth::Backend::new(db);
    // let auth_layer = AuthManagerLayerBuilder::new(backend, session_layer).build();

    let app = routes::configure(user_db).with_state(api_state);

    // Create tmp dir for caching
    let tmp = env::temp_dir();
    match fs::create_dir_all(&tmp.join("sgwebserver")) {
        Ok(_) => println!("Created tmp dir"),
        Err(e) => panic!("Error creating tmp dir: {}", e),
    };

    // Update images
    routes::update_images().await;

    //let api_db = SqlitePool::connect("sqlite:db/api.db").await.unwrap();
    //let api_service = ServiceBuilder::new().layer(AddExtension(api_db)).finish();

    //let app: Router = Router::new()
    //.route("/", get(root))
    //.route("/images", get(images))
    //.merge(sg_api::api_routes())
    //.nest_service("/assets", ServeDir::new("assets"))
    //.route_layer(login_required!(sg_auth::Backend, login_url = "/login"))
    //.route("/login", get(login_page))
    //.route("/login", post(sg_auth::login))
    //.layer(auth_layer)
    //.route("/upload", post(upload))
    //.fallback(unknown_route);

    let listener = TcpListener::bind(&address).await.unwrap();

    println!("Listening on: {}", address);

    axum::serve(listener, app).await.unwrap();
}
