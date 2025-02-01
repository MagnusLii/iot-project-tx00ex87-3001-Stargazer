use crate::{
    auth::login::AuthSession,
    keys,
    web::{commands, diagnostics, images},
    SharedState,
};
use axum::{
    extract::{rejection::QueryRejection, Query, State},
    http::StatusCode,
    response::{Html, IntoResponse},
};
use serde::Deserialize;

pub async fn root(State(state): State<SharedState>) -> impl IntoResponse {
    let mut html = std::include_str!("../../html/home.html").to_string();

    if let Ok(next_pic) = commands::get_next_pic_estimate(&state.db).await {
        let html_next_pic = format!("{} at {}", next_pic.target, next_pic.datetime);
        html = html.replace("<!--NEXT_PIC-->", &html_next_pic);
    } else {
        html = html.replace("<!--NEXT_PIC-->", "No estimates currently available");
    }

    if let Ok(last_pic) = images::get_last_image(&state.db).await {
        let html_last_pic = format!("src=\"{}\" alt=\"{}\"", last_pic.web_path, last_pic.name);
        html = html.replace("<!--LAST_PIC-->", &html_last_pic);
    } else {
        html = html.replace("<!--LAST_PIC-->", "No images currently available");
    }

    (StatusCode::OK, Html(html))
}

#[derive(Debug, Deserialize)]
pub struct GalleryQuery {
    page: Option<u32>,
    psize: Option<u32>,
}
pub async fn gallery(
    State(state): State<SharedState>,
    query: Result<Query<GalleryQuery>, QueryRejection>,
) -> impl IntoResponse {
    let mut page: u32 = 1;
    let mut page_size: u32 = 8;

    match query {
        Ok(q) => {
            let params = q.0;
            println!("Query: {:?}", params);

            if let Some(p) = params.page {
                if p > 0 {
                    page = p
                }
            }
            if let Some(sz) = params.psize {
                if sz >= 4 && sz <= 24 {
                    page_size = sz
                }
            }
        }
        Err(e) => eprintln!("Error parsing query: {}", e),
    }

    let mut html = include_str!("../../html/images.html").to_string();

    images::check_images(&state.db, &state.image_dir, false)
        .await
        .unwrap();

    if let Ok(count) = images::get_image_count(&state.db).await {
        let mut last_on_page = (page * page_size) as u64;
        if last_on_page > count {
            last_on_page = count;
        }

        html = html.replace("<!--COUNT-->", &count.to_string());

        let total_pages = (count + page_size as u64 - 1) / page_size as u64;

        let page_text = format!(
            "data-page=\"{}\" data-pages=\"{}\">{} - {} (of {} total)",
            page,
            total_pages,
            (page - 1) * page_size + 1,
            last_on_page,
            count
        );
        html = html.replace("<!--PAGE_TEXT-->", &page_text);
    } else {
        html = html.replace("<!--COUNT-->", "??");
        html = html.replace(
            "<!--PAGE_TEXT-->",
            "data-page=\"1\" data-pages=\"1\">? - ? (of ?? total)",
        );
    }

    let psize_selected = format!(
        "<option value=\"{}\" disabled selected>{}</option>",
        page_size, page_size
    );
    html = html.replace("<!--PSIZE-->", &psize_selected);

    match images::get_image_info(&state.db, page, page_size).await {
        Ok(images) => {
            if images.len() > 0 {
                let html_images = images
                    .iter()
                    .map(|image| {
                        format!(
                            "<img class=\"photo\" src=\"{}\" alt=\"{}\"/>",
                            image.web_path, image.name
                        )
                    })
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

    if let Ok(keys) = keys::get_keys(&state.db).await {
        let html_keys = keys
            .iter()
            .map(|key| format!("<option value=\"{}\">{}</option>", key.id, key.name))
            .collect::<Vec<String>>()
            .join("\n");
        html = html.replace("<!--API_KEYS-->", &html_keys);
    } else {
        html = html.replace(
            "<!--API_KEYS-->",
            "<option value=\"-1\" disabled>Error loading options</option>",
        );
    }

    if let Ok(targets) = commands::get_target_names(&state.db).await {
        let html_targets = targets
            .iter()
            .map(|target| format!("<option value=\"{}\">{}</option>", target.id, target.name))
            .collect::<Vec<String>>()
            .join("\n");
        html = html.replace("<!--TARGETS-->", &html_targets);
    } else {
        html = html.replace(
            "<!--TARGETS-->",
            "<option value=\"-1\" disabled>Error loading options</option>",
        );
    }

    if let Ok(positions) = commands::get_target_positions(&state.db).await {
        let html_positions = positions
            .iter()
            .map(|position| {
                format!(
                    "<option value=\"{}\">{}</option>",
                    position.id, position.position
                )
            })
            .collect::<Vec<String>>()
            .join("\n");
        html = html.replace("<!--POSITIONS-->", &html_positions);
    } else {
        html = html.replace(
            "<!--POSITIONS-->",
            "<option value=\"-1\" disabled>Error loading options</option>",
        );
    }

    if let Ok(statistics) = commands::fetch_command_statistics(&state.db).await {
        html = html.replace("<!--COMPLETED-->", &statistics.completed.to_string());
        html = html.replace("<!--FAILED-->", &statistics.failed.to_string());
        html = html.replace("<!--IN_PROGRESS-->", &statistics.in_progress.to_string());
    } else {
        html = html.replace("<!--COMPLETED-->", "??");
        html = html.replace("<!--FAILED-->", "??");
        html = html.replace("<!--IN_PROGRESS-->", "??");
    }

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

    let html_diagnostics = diagnostics::get_diagnostics(name.name, &state.db)
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
    if let Some(user) = auth_session.user {
        if user.superuser != true {
            return (
                StatusCode::FORBIDDEN,
                Html(include_str!("../../html/403.html").to_string()),
            );
        }
    }

    let mut html = include_str!("../../html/user_management.html").to_string();

    if let Ok(users) = auth_session.backend.get_users().await {
        let html_users = users
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
    } else {
        html = html.replace("<!--USERS-->", "<section>Error loading users</section>");
    }

    (StatusCode::OK, Html(html))
}

pub async fn user_page(auth_session: AuthSession) -> impl IntoResponse {
    let mut html = include_str!("../../html/user.html").to_string();

    if let Some(user) = auth_session.user {
        html = html.replace("<!--NAME-->", &user.username);
    } else {
        html = html.replace("<!--NAME-->", "???");
    }

    (StatusCode::OK, Html(html))
}

pub async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../../html/404.html")),
    )
}

pub async fn test(messages: axum_messages::Messages) -> impl IntoResponse {
    let html = include_str!("../../html/images.html").to_string();

    messages.error("Test error");

    (StatusCode::OK, Html(html))
}
