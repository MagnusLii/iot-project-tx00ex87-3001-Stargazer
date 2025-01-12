use crate::{
    auth::login::AuthSession,
    keys,
    web::{commands::get_commands, diagnostics::get_diagnostics, images},
    SharedState,
};
use axum::{
    extract::{Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
};
use serde::Deserialize;

pub async fn root() -> impl IntoResponse {
    let html = std::include_str!("../../html/index.html").to_string();

    (StatusCode::OK, Html(html))
}

#[derive(Debug, Deserialize)]
pub struct GalleryQuery {
    page: Option<u32>,
    psize: Option<u32>,
}
pub async fn gallery(
    State(state): State<SharedState>,
    Query(query): Query<GalleryQuery>,
) -> impl IntoResponse {
    let mut html = include_str!("../../html/images.html").to_string();
    println!("Query: {:?}", query);

    images::check_images(&state.db, &state.image_dir, false)
        .await
        .unwrap();

    let count = images::get_image_count(&state.db).await.unwrap();
    html = html.replace("<!--COUNT-->", &count.to_string());

    let mut page: u32 = 1;
    let mut page_size: u32 = 10;

    if let Some(p) = query.page {
        if p > 0 {
            page = p
        }
    }
    if let Some(p) = query.psize {
        if p > 0 || p <= 25 {
            page_size = p
        }
    }

    match images::get_image_info(&state.db, page, page_size).await {
        Ok(images) => {
            if images.len() > 0 {
                let html_images = images
                    .iter()
                    .map(|image| format!("<img src=\"{}\"/>", image.web_path))
                    .collect::<Vec<String>>()
                    .join("\n");

                html = html.replace("<!--IMAGES-->", &html_images);
            } else {
                html = html.replace("<!--IMAGES-->", "<p>No results</p>");
            }
            (StatusCode::OK, Html(html))
        }
        Err(e) => {
            eprintln!("Error getting images: {}", e);
            html = html.replace("<!--IMAGES-->", "<p>Error getting images</p>");
            (StatusCode::INTERNAL_SERVER_ERROR, Html(html))
        }
    }
}

pub async fn control(State(state): State<SharedState>) -> impl IntoResponse {
    let mut html = std::include_str!("../../html/control.html").to_string();
    let html_keys = keys::get_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| format!("<option value=\"{}\">{}</option>", key.id, key.name))
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    let html_commands = get_commands(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|command| {
            format!(
                "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                command.id,
                command.target,
                command.position,
                command.name,
                command.associated_key,
                command.status
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_COMMANDS-->", &html_commands);

    (StatusCode::OK, Html(html))
}

pub async fn api_keys(State(state): State<SharedState>) -> impl IntoResponse {
    let mut html = include_str!("../../html/keys.html").to_string();
    let html_keys = keys::get_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| {
            format!(
                "<li value=\"{}\">{}: {}</li><button onclick=\"deleteKey({})\">Delete</button>",
                key.id, key.name, key.api_token, key.id
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    (StatusCode::OK, Html(html))
}

#[derive(Debug, Deserialize)]
pub struct DiagnosticQuery {
    name: Option<String>,
}

pub async fn diagnostics(
    State(state): State<SharedState>,
    Query(name): Query<DiagnosticQuery>,
) -> impl IntoResponse {
    let mut html = include_str!("../../html/diagnostics.html").to_string();

    let html_diagnostics = get_diagnostics(name.name, &state.db)
        .await
        .unwrap()
        .iter()
        .map(|diagnostic| {
            format!(
                "<tr><td>{}</td><td>{}</td><td>{}</td></tr>",
                diagnostic.name, diagnostic.status, diagnostic.message
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--DIAGNOSTICS-->", &html_diagnostics);

    (StatusCode::OK, Html(html))
}

pub async fn user_management(auth_session: AuthSession) -> impl IntoResponse {
    if auth_session.user.unwrap().superuser != true {
        return (
            StatusCode::FORBIDDEN,
            Html(include_str!("../../html/403.html").to_string()),
        );
    }
    let mut html = include_str!("../../html/user_management.html").to_string();

    let html_users = auth_session
        .backend
        .get_users()
        .await
        .unwrap()
        .iter()
        .map(|user| {
            format!(
                "<li value=\"{}\">{}<button onclick=\"deleteUser({})\">Delete</button>",
                user.id, user.username, user.id
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--USERS-->", &html_users);

    (StatusCode::OK, Html(html))
}

pub async fn user_page(auth_session: AuthSession) -> impl IntoResponse {
    let html = include_str!("../../html/user.html").to_string();

    let name = auth_session.user.unwrap().username;
    let html = html.replace("<!--NAME-->", &name);

    (StatusCode::OK, Html(html))
}

pub async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../../html/404.html")),
    )
}

pub async fn test() -> impl IntoResponse {
    let html = include_str!("../../html/images.html").to_string();

    (StatusCode::OK, Html(html))
}
