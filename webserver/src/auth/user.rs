use crate::{
    auth::{backend::Credentials, login::AuthSession, password},
    err::Error,
};
use axum::{
    body::Body,
    extract::{Json, Query},
    http::StatusCode,
    response::{IntoResponse, Response},
};
use axum_login::AuthUser;
use serde::{Deserialize, Serialize};
use sqlx::FromRow;

/// Represents a user in the system.
#[derive(Debug, Clone, Serialize, Deserialize, FromRow)]
pub struct User {
    /// The unique identifier of the user.
    pub id: i64,
    /// The username of the user.
    pub username: String,
    /// The hashed password of the user.
    pub password: String,
    /// Indicates if the user is a superuser.
    pub superuser: bool,
}

impl AuthUser for User {
    type Id = i64;

    /// Returns the user's ID.
    fn id(&self) -> Self::Id {
        self.id
    }

    /// Returns the user's password as a byte slice for session authentication.
    fn session_auth_hash(&self) -> &[u8] {
        self.password.as_bytes()
    }
}

/// Retrieves the details of the currently authenticated user.
///
/// # Arguments
///
/// * `auth_session`: The authentication session.
///
/// # Returns
///
/// A JSON formatted response containing the user details or an empty object if not authenticated.
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

/// Adds a new user to the system.
///
/// # Arguments
///
/// * `auth_session`: The authentication session.
/// * `user`: The credentials of the new user.
///
/// # Returns
///
/// A HTTP response indicating the success or failure of the operation.
pub async fn new_user(auth_session: AuthSession, user: Json<Credentials>) -> impl IntoResponse {
    if auth_session.user.unwrap().superuser != true {
        return (StatusCode::FORBIDDEN, "Not authorized").into_response();
    }

    if !user.username.chars().all(char::is_alphanumeric) {
        return (StatusCode::BAD_REQUEST, "Invalid username").into_response();
    }
    if !user.password.chars().all(password::is_valid_char) {
        return (StatusCode::BAD_REQUEST, "Invalid password").into_response();
    }

    println!("Attempting to add user: {}", &user.username);
    match auth_session.backend.add_user(user.0).await {
        Ok(_) => (StatusCode::OK, "User added").into_response(),
        Err(e) => user_mod_err_handling(e),
    }
}

/// Represents the query parameters for removing a user.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemoveUserQuery {
    /// The ID of the user to remove.
    pub id: i64,
}

/// Removes a user from the system.
///
/// # Arguments
///
/// * `auth_session`: The authentication session.
/// * `user`: The query parameters containing the ID of the user to remove.
///
/// # Returns
///
/// A HTTP response indicating the success or failure of the operation.
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
            eprintln!("Error removing user");
            (StatusCode::INTERNAL_SERVER_ERROR, "Error removing user").into_response()
        }
    }
}

/// Represents the JSON payload for modifying user details.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModifyUserJson {
    /// The ID of the user to modify. If `None`, the current user's ID is used.
    pub id: Option<i64>,
    /// The new username of the user.
    pub username: Option<String>,
    /// The new password of the user.
    pub password: Option<String>,
}

/// Modifies the details of a user.
///
/// # Arguments
///
/// * `auth_session`: The authentication session.
/// * `user`: The JSON payload containing the user details to modify.
///
/// # Returns
///
/// A HTTP response indicating the success or failure of the operation.
pub async fn modify_user(
    auth_session: AuthSession,
    user: Json<ModifyUserJson>,
) -> impl IntoResponse {
    let modified_details = user.0;
    let user_session = auth_session.user.unwrap_or_else(|| User {
        id: -1,
        username: "".to_string(),
        password: "".to_string(),
        superuser: false,
    });
    if user_session.id == -1 {
        return (StatusCode::FORBIDDEN, "Not logged in").into_response();
    }

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

        match auth_session
            .backend
            .change_username(target_user_id, new_username)
            .await
        {
            Ok(_) => println!("Changed username of user: {}", target_user_id),
            Err(e) => return user_mod_err_handling(e),
        }
    }

    // Change password if provided
    if let Some(new_password) = modified_details.password {
        if !new_password.is_empty() {
            if !new_password.chars().all(password::is_valid_char) {
                return (StatusCode::BAD_REQUEST, "Invalid password").into_response();
            }

            if let Ok(_) = auth_session
                .backend
                .change_password(target_user_id, new_password)
                .await
            {
                println!("Changed password of user {}", target_user_id);
            } else {
                return (StatusCode::INTERNAL_SERVER_ERROR, "Error changing password")
                    .into_response();
            }
        }
    }

    (StatusCode::OK, "User modified").into_response()
}

/// Handles errors that occur when modifying a user.
/// 
/// # Arguments
/// * `err` - The error that occurred.
/// 
/// # Returns
/// A HTTP response indicating the error.
fn user_mod_err_handling(err: Error) -> Response<Body> {
    match err {
        Error::Sqlx(e) => match e {
            sqlx::Error::Database(e) => {
                if e.is_unique_violation() {
                    (StatusCode::BAD_REQUEST, "Username already in use").into_response()
                } else {
                    (StatusCode::INTERNAL_SERVER_ERROR, "Error adding user").into_response()
                }
            }
            _ => (StatusCode::INTERNAL_SERVER_ERROR, "Error adding user").into_response(),
        },
        _ => (StatusCode::INTERNAL_SERVER_ERROR, "Error adding user").into_response(),
    }
}
