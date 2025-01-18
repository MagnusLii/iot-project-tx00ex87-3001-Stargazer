use crate::auth::backend::{Backend, Credentials};
use axum::{
    debug_handler,
    extract::Query,
    http::StatusCode,
    response::{Html, IntoResponse, Redirect},
    Form,
};
use serde::Deserialize;

pub type AuthSession = axum_login::AuthSession<Backend>;

#[derive(Debug, Deserialize)]
pub struct Next {
    pub next: Option<String>,
    pub fail: Option<u8>,
}

#[debug_handler]
pub async fn login(
    mut auth_session: AuthSession,
    Form(creds): Form<Credentials>,
) -> impl IntoResponse {
    println!("Attempting login: {}", creds.username);
    let user = match auth_session.authenticate(creds.clone()).await {
        Ok(Some(user)) => user,
        Ok(None) => {
            let mut url = String::from("/login");
            if let Some(next_url) = creds.next {
                url = format!("{}?next={}&fail=1", url, next_url);
            }

            return Redirect::to(&url).into_response();
        }
        Err(_) => {
            return (StatusCode::INTERNAL_SERVER_ERROR, "Error authenticating").into_response()
        }
    };

    if auth_session.login(&user).await.is_err() {
        return (StatusCode::INTERNAL_SERVER_ERROR, "Error logging in").into_response();
    }
    println!("User logged in: {}", user.username);

    if let Some(next) = creds.next {
        println!("Redirecting to: {}", next);
        Redirect::to(&next).into_response()
    } else {
        Redirect::to("/").into_response()
    }
}

pub async fn logout(mut auth_session: AuthSession) -> impl IntoResponse {
    match auth_session.logout().await {
        Ok(_) => (),
        Err(_) => return StatusCode::INTERNAL_SERVER_ERROR.into_response(),
    }
    Redirect::to("/").into_response()
}

pub async fn login_page(auth_session: AuthSession, Query(url): Query<Next>) -> impl IntoResponse {
    if auth_session.user.is_some() {
        Redirect::to("/").into_response()
    } else {
        let mut html = std::include_str!("../../html/login.html").to_string();
        html = html.replace("<!--NEXT-->", &url.next.unwrap_or("".to_string()));

        if let Some(_) = url.fail {
            html = html.replace("<!--ERROR-->", "username or password is incorrect");
        }

        (StatusCode::OK, Html(html)).into_response()
    }
}
