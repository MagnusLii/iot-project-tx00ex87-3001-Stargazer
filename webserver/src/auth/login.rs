use crate::auth::backend::{Backend, Credentials};
use axum::{
    debug_handler,
    http::StatusCode,
    response::{Html, IntoResponse, Redirect},
    Form,
};
use axum_login::AuthUser;

pub type AuthSession = axum_login::AuthSession<Backend>;

#[debug_handler]
pub async fn login(
    mut auth_session: AuthSession,
    Form(creds): Form<Credentials>,
) -> impl IntoResponse {
    println!("Attempting login: {:?}", creds);
    let user = match auth_session.authenticate(creds.clone()).await {
        Ok(Some(user)) => user,
        Ok(None) => return (StatusCode::UNAUTHORIZED, "Invalid credentials").into_response(),
        Err(_) => {
            return (StatusCode::INTERNAL_SERVER_ERROR, "Error authenticating").into_response()
        }
    };

    println!("User authenticated: {}", user.id());
    if auth_session.login(&user).await.is_err() {
        return StatusCode::INTERNAL_SERVER_ERROR.into_response();
    }
    println!("User logged in: {}", user.id());
    Redirect::to("/").into_response()
}

pub async fn login_page() -> impl IntoResponse {
    (
        StatusCode::OK,
        Html(std::include_str!("../../html/login.html")),
    )
}
