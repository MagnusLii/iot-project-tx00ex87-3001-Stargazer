use crate::auth::{backend::Credentials, login::AuthSession};
use axum::{
    extract::{Json, Query},
    http::StatusCode,
    response::IntoResponse,
};
use axum_login::AuthUser;
use serde::{Deserialize, Serialize};
use sqlx::FromRow;

#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct User {
    pub id: i64,
    pub username: String,
    pub password: String,
    pub superuser: bool,
}

impl AuthUser for User {
    type Id = i64;

    fn id(&self) -> Self::Id {
        self.id
    }

    fn session_auth_hash(&self) -> &[u8] {
        self.password.as_bytes()
    }
}

pub async fn current_user(auth_session: AuthSession) -> impl IntoResponse {
    let user_details: String;
    if auth_session.user.is_some() {
        let user = auth_session.user.unwrap();
        user_details = format!(r#"{{"username": "{}", "id": {}}}"#, user.username, user.id);
    } else {
        user_details = "{}".to_string();
    }

    (StatusCode::OK, user_details)
}

pub async fn new_user(auth_session: AuthSession, user: Json<Credentials>) -> impl IntoResponse {
    if auth_session.user.unwrap().superuser != true {
        return (StatusCode::FORBIDDEN, "Not authorized").into_response();
    }

    if !user.username.chars().all(char::is_alphanumeric) {
        return (StatusCode::BAD_REQUEST, "Invalid username").into_response();
    }
    if !user.password.chars().all(char::is_alphanumeric) {
        return (StatusCode::BAD_REQUEST, "Invalid password").into_response();
    }

    println!("Attempting to add user: {}", user.username);
    if let Ok(_) = auth_session.backend.add_user(user.0).await {
        (StatusCode::OK, "User added").into_response()
    } else {
        (StatusCode::INTERNAL_SERVER_ERROR, "Error adding user").into_response()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemoveUserQuery {
    pub id: i64,
}

#[axum::debug_handler]
pub async fn remove_user(
    auth_session: AuthSession,
    user: Query<RemoveUserQuery>,
) -> impl IntoResponse {
    let user_session = auth_session.user.unwrap();
    if user_session.superuser != true {
        return (StatusCode::FORBIDDEN, "Not authorized").into_response();
    }

    println!("Attempting to remove user: {}", user.id);
    if user_session.id == user.id {
        (StatusCode::FORBIDDEN, "Can't remove yourself").into_response()
    } else {
        if let Ok(_) = auth_session.backend.delete_user(user.id).await {
            (StatusCode::OK, "User removed").into_response()
        } else {
            (StatusCode::INTERNAL_SERVER_ERROR, "Error removing user").into_response()
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModifyUserJson {
    pub id: Option<i64>,
    pub username: Option<String>,
    pub password: Option<String>,
}

pub async fn modify_user(
    auth_session: AuthSession,
    user: Json<ModifyUserJson>,
) -> impl IntoResponse {
    let modified_details = user.0;
    let user_session = auth_session.user.unwrap();

    let target_user_id;
    if modified_details.id.is_none() {
        target_user_id = user_session.id;
    } else {
        target_user_id = modified_details.id.unwrap();
    }
    if user_session.id != target_user_id {
        if user_session.superuser != true {
            return (StatusCode::FORBIDDEN, "Not authorized").into_response();
        }
    }

    println!("Attempting to modify user: {}", target_user_id);
    // Change username if provided
    if let Some(new_username) = modified_details.username {
        if !new_username.chars().all(char::is_alphanumeric) || new_username.is_empty() {
            return (StatusCode::BAD_REQUEST, "Invalid username").into_response();
        }

        if let Ok(_) = auth_session
            .backend
            .change_username(target_user_id, new_username)
            .await
        {
            println!("Changed username of user {}", target_user_id);
        } else {
            return (StatusCode::INTERNAL_SERVER_ERROR, "Error changing username").into_response();
        }
    }

    // Change password if provided
    if let Some(new_password) = modified_details.password {
        if !new_password.chars().all(char::is_alphanumeric) || new_password.is_empty() {
            return (StatusCode::BAD_REQUEST, "Invalid password").into_response();
        }

        if let Ok(_) = auth_session
            .backend
            .change_password(target_user_id, new_password)
            .await
        {
            println!("Changed password of user {}", target_user_id);
        } else {
            return (StatusCode::INTERNAL_SERVER_ERROR, "Error changing password").into_response();
        }
    }

    (StatusCode::OK, "User modified").into_response()
}
