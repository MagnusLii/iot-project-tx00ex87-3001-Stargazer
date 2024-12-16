use crate::{
    api::{commands, keys, ApiState},
    web::{diagnostics::get_diagnostics, images},
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
    (
        StatusCode::OK,
        Html(std::include_str!("../html/index.html")),
    )
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
    let mut html = std::include_str!("../html/control.html").to_string();
    let html_keys = keys::get_api_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| format!("<option value=\"{}\">{}</option>", key.id, key.name))
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_KEYS-->", &html_keys);

    let html_commands = commands::get_commands(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|command| {
            format!(
                "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                command.id, command.target, command.name, command.associated_key
            )
        })
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--API_COMMANDS-->", &html_commands);

    (StatusCode::OK, Html(html))
}

pub async fn api_keys(State(state): State<ApiState>) -> impl IntoResponse {
    let mut html = include_str!("../html/keys.html").to_string();
    let html_keys = keys::get_api_keys(&state.db)
        .await
        .unwrap()
        .iter()
        .map(|key| {
            format!(
                "<li value=\"{}\">{}: {}</li><button onclick=\"deleteKey({})\">Delete</button>",
                key.id, key.name, key.api_key, key.id
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
    let mut html = include_str!("../html/diagnostics.html").to_string();

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

pub async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../html/404.html")),
    )
}
