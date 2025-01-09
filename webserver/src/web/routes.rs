use crate::{
    api::ApiState,
    auth::login::AuthSession,
    keys,
    web::{commands::get_commands, diagnostics::get_diagnostics, images},
};
use axum::{
    extract::{Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
};
use serde::Deserialize;
use std::env;
use tokio::fs;

pub async fn root() -> impl IntoResponse {
    let html = std::include_str!("../../html/index.html").to_string();

    (StatusCode::OK, Html(html))
}

pub async fn gallery() -> impl IntoResponse {
    let tmp = env::temp_dir();

    if let Ok(cached_html) = fs::read(&tmp.join("sgwebserver/images.html")).await {
        return (StatusCode::OK, Html(cached_html));
    }

    let updated = images::update_gallery().await;

    let status: StatusCode;
    let html: Vec<u8>;

    if !updated {
        status = StatusCode::INTERNAL_SERVER_ERROR;
        html = "Error updating images".as_bytes().to_vec();
    } else {
        status = StatusCode::OK;
        html = fs::read(&tmp.join("sgwebserver/images.html"))
            .await
            .unwrap();
    }

    (status, Html(html.into()))
}

pub async fn control(State(state): State<ApiState>) -> impl IntoResponse {
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
                "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                command.id,
                command.target,
                command.position,
                command.name,
                command.associated_key,
                command.status,
                command.time
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_COMMANDS-->", &html_commands);

    (StatusCode::OK, Html(html))
}

pub async fn api_keys(State(state): State<ApiState>) -> impl IntoResponse {
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
    State(state): State<ApiState>,
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

pub async fn test(auth_session: AuthSession) -> impl IntoResponse {
    let mut html = include_str!("../../html/index.html").to_string();
    if auth_session.user.is_some() {
        let msg = format!("User: {}", auth_session.user.unwrap().username);
        html = html.replace("<!--USER-->", &msg);
        (StatusCode::OK, Html(html))
    } else {
        (
            StatusCode::UNAUTHORIZED,
            Html("You are not logged in.".to_string()),
        )
    }
}
