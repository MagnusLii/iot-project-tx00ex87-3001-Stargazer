use crate::{
    auth::login::AuthSession,
    keys,
    web::{commands, diagnostics, images},
    SharedState,
};
use axum::{
    extract::{rejection::QueryRejection, Query, State},
    http::StatusCode,
    response::{Html, IntoResponse, Redirect},
};
use serde::Deserialize;

/// The root route for the webserver
/// 
/// # Arguments
/// * `state` - The shared state of the webserver
///
/// # Returns
/// * A HTTP response containing the home page
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
        html = html.replace("<!--LAST_PIC-->", "alt=\"No images currently available\"");
    }

    (StatusCode::OK, Html(html))
}

/// Represents a query for the gallery route
#[derive(Debug, Deserialize)]
pub struct GalleryQuery {
    /// Optional page number
    page: Option<u32>,
    /// Optional page size
    psize: Option<u32>,
}

/// The gallery route for the webserver
/// 
/// # Arguments
/// * `state` - The shared state of the webserver
/// * `query` - The query parameters for the gallery route
/// 
/// # Returns
/// * A HTTP response containing the gallery page
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

/// The control route for the webserver
/// 
/// # Arguments
/// * `state` - The shared state of the webserver
/// 
/// # Returns
/// * A HTTP response containing the control commands page
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

/// The keys route for the webserver
/// 
/// # Arguments
/// * `state` - The shared state of the webserver
/// 
/// # Returns
/// * A HTTP response containing the API keys page
pub async fn api_keys(State(state): State<SharedState>) -> impl IntoResponse {
    let mut html = include_str!("../../html/keys.html").to_string();

    let html_keys = match keys::get_keys(&state.db).await {
        Ok(keys) => {
            if keys.len() == 0 {
                "<tr><td colspan=\"4\">No keys found</td></tr>".to_string()
            } else {
                keys.iter().map(|key| {
                    format!(
                        "<tr><td>{}</td><td>{}</td><td>{}</td><td><button onclick=\"deleteKey({})\">Delete</button></td></tr>",
                        key.id, key.name, key.api_token, key.id
                    )
                })
                .collect::<Vec<String>>()
                .join("\n")
            }
        }
        Err(e) => {
            eprintln!("Error getting keys: {}", e);
            "<tr><td colspan=\"4\">Error getting keys</td></tr>".to_string()
        }
    };

    html = html.replace("<!--API_KEYS-->", &html_keys);

    (StatusCode::OK, Html(html))
}

/// Represents a query for the diagnostics route
#[derive(Debug, Deserialize)]
pub struct DiagnosticQuery {
    /// Optional name filter
    name: Option<String>,
    /// Optional page number
    page: Option<u32>,
    /// Optional status filter
    status: Option<u8>,
}

/// The diagnostics route for the webserver
/// 
/// # Arguments
/// * `state` - The shared state of the webserver
/// * `query` - The query parameters for the diagnostics route
/// 
/// # Returns
/// * A HTTP response containing the diagnostics page
pub async fn diagnostics(
    State(state): State<SharedState>,
    query: Result<Query<DiagnosticQuery>, QueryRejection>,
) -> impl IntoResponse {
    let mut html = include_str!("../../html/diagnostics.html").to_string();

    let query = match query {
        Ok(q) => q.0,
        Err(e) => {
            eprintln!("Error parsing query: {}", e);
            DiagnosticQuery {
                name: None,
                page: None,
                status: None,
            }
        }
    };

    let mut valid_query = false;
    if let Ok(devices) = diagnostics::get_diagnostics_names(&state.db).await {
        let mut html_devices = "<option value=\"\">All</option>".to_string();

        if let Some(name) = &query.name {
            html_devices += &devices
                .iter()
                .map(|device| {
                    if &device.name == name {
                        valid_query = true;
                        format!(
                            "<option value=\"{}\" selected>{}</option>",
                            device.name, device.name
                        )
                    } else {
                        format!("<option value=\"{}\">{}</option>", device.name, device.name)
                    }
                })
                .collect::<Vec<String>>()
                .join("\n");
        } else {
            valid_query = true;
            html_devices += &devices
                .iter()
                .map(|device| format!("<option value=\"{}\">{}</option>", device.name, device.name))
                .collect::<Vec<String>>()
                .join("\n");
        }
        html = html.replace("<!--NAMES-->", &html_devices);
    }

    if !valid_query {
        html = html.replace("<!--STATUSES-->", diagnostics::STATUS_DEFAULT);
        html = html.replace("<!--PAGINATION-->", "? / ??");
        html = html.replace(
            "<!--DIAGNOSTICS-->",
            "<tr><td colspan=\"4\">Invalid name filter</td></tr>",
        );
    } else if let Ok(diagnostics) =
        diagnostics::get_diagnostics(query.name, query.page, query.status, &state.db).await
    {
        let html_diagnostics = if !diagnostics.data.is_empty() {
            diagnostics
                .data
                .iter()
                .map(|diagnostic| {
                    format!(
                        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                        diagnostic.name, diagnostic.status, diagnostic.message, diagnostic.datetime
                    )
                })
                .collect::<Vec<String>>()
                .join("\n")
        } else {
            "<tr><td colspan=\"4\">No diagnostics found</td></tr>".to_string()
        };

        html = html.replace("<!--DIAGNOSTICS-->", &html_diagnostics);

        let html_count = format!(
            "<a id=\"page_count\" class=\"not-link\" data-page=\"{}\" data-pages=\"{}\">{} / {}</a>",
            diagnostics.page, diagnostics.pages, diagnostics.page, diagnostics.pages
        );

        let html_statuses = if let Some(status) = query.status {
            match status {
                1 => "<option value=\"\">All</option><option value=\"1\" selected>Info</option><option value=\"2\">Warning</option><option value=\"3\">Error</option>".to_string(),
                2 => "<option value=\"\">All</option><option value=\"1\">Info</option><option value=\"2\" selected>Warning</option><option value=\"3\">Error</option>".to_string(),
                3 => "<option value=\"\">All</option><option value=\"1\">Info</option><option value=\"2\">Warning</option><option value=\"3\" selected>Error</option>".to_string(),
                _ => diagnostics::STATUS_DEFAULT.to_string(),
            }
        } else {
            diagnostics::STATUS_DEFAULT.to_string()
        };

        html = html.replace("<!--STATUSES-->", &html_statuses);
        html = html.replace("<!--PAGINATION-->", &html_count);
        html = html.replace("<!--DIAGNOSTICS-->", &html_diagnostics);
    } else {
        html = html.replace("<!--STATUSES-->", diagnostics::STATUS_DEFAULT);
        html = html.replace("<!--PAGINATION-->", "? / ??");
        html = html.replace(
            "<!--DIAGNOSTICS-->",
            "<tr><td colspan=\"4\">Could not load diagnostics</td></tr>",
        );
    }

    (StatusCode::OK, Html(html))
}

/// The users route for the webserver
/// 
/// # Arguments
/// * `auth_session` - The authentication session
/// 
/// # Returns
/// * A HTTP response containing the user management page
/// * A HTTP response containing the forbidden page
pub async fn user_management(auth_session: AuthSession) -> impl IntoResponse {
    let forbidden = include_str!("../../html/user_management_403.html").to_string();
    if let Some(user) = auth_session.user {
        if user.superuser != true {
            return (StatusCode::FORBIDDEN, Html(forbidden));
        }
    } else {
        return (StatusCode::FORBIDDEN, Html(forbidden));
    }

    let mut html = include_str!("../../html/user_management.html").to_string();

    if let Ok(admins) = auth_session.backend.get_users(Some(true)).await {
        let html_admins = admins
            .iter()
            .map(|user| format!("<li value=\"{}\">{}</li>", user.id, user.username))
            .collect::<Vec<String>>()
            .join("\n");
        html = html.replace("<!--ADMINS-->", &html_admins);
    } else {
        html = html.replace("<!--ADMINS-->", "<section>Error loading admins</section>");
    }

    if let Ok(users) = auth_session.backend.get_users(Some(false)).await {
        let html_users = users
            .iter()
            .map(|user| {
                format!(
                    "<li value=\"{}\">{}<button onclick=\"deleteUser({})\">Delete</button></li>",
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

/// The user route for the webserver
/// 
/// # Arguments
/// * `auth_session` - The authentication session
/// 
/// # Returns
/// * A HTTP response containing the current user page
pub async fn user_page(auth_session: AuthSession) -> impl IntoResponse {
    let mut html = include_str!("../../html/user.html").to_string();

    if let Some(user) = auth_session.user {
        html = html.replace("<!--NAME-->", &user.username);
    } else {
        html = html.replace("<!--NAME-->", "???");
    }

    (StatusCode::OK, Html(html))
}

/// Route to redirect to the favicon
/// 
/// # Returns
/// * A HTTP response redirecting to the favicon
pub async fn favicon() -> impl IntoResponse {
    Redirect::to("/assets/favicon.ico")
}

/// Route to handle unknown routes
/// 
/// # Returns
/// * A HTTP response containing the 404 page
pub async fn unknown_route() -> impl IntoResponse {
    (
        StatusCode::NOT_FOUND,
        Html(std::include_str!("../../html/404.html")),
    )
}
