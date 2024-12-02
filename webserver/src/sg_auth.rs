use std::collections::HashMap;

use async_trait::async_trait;
use axum_login::{AuthUser, AuthnBackend, UserId};

use axum::{
    debug_handler,
    http::StatusCode,
    response::{IntoResponse, Redirect},
    Form,
};
use serde::Deserialize;

type AuthSession = axum_login::AuthSession<Backend>;

#[derive(Debug, Clone)]
pub struct User {
    id: i64,
    pw_hash: Vec<u8>,
}

impl AuthUser for User {
    type Id = i64;

    fn id(&self) -> Self::Id {
        self.id
    }

    fn session_auth_hash(&self) -> &[u8] {
        &self.pw_hash
    }
}

#[derive(Clone, Default)]
pub struct Backend {
    users: HashMap<i64, User>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct Credentials {
    pub user_id: i64,
}

#[async_trait]
impl AuthnBackend for Backend {
    type User = User;
    type Credentials = Credentials;
    type Error = std::convert::Infallible;

    async fn authenticate(
        &self,
        Credentials { user_id }: Self::Credentials,
    ) -> Result<Option<Self::User>, Self::Error> {
        Ok(self.users.get(&user_id).cloned())
    }

    async fn get_user(&self, user_id: &UserId<Self>) -> Result<Option<Self::User>, Self::Error> {
        Ok(self.users.get(user_id).cloned())
    }
}

#[debug_handler]
pub async fn login(
    mut auth_session: AuthSession,
    Form(creds): Form<Credentials>,
) -> impl IntoResponse {
    println!("Attempting login: {:?}", creds);
    let user = match auth_session.authenticate(creds.clone()).await {
        Ok(Some(user)) => user,
        Ok(None) => return StatusCode::UNAUTHORIZED.into_response(),
        Err(_) => return StatusCode::INTERNAL_SERVER_ERROR.into_response(),
    };

    println!("User authenticated: {}", user.id());
    if auth_session.login(&user).await.is_err() {
        return StatusCode::INTERNAL_SERVER_ERROR.into_response();
    }
    println!("User logged in: {}", user.id());
    Redirect::to("/images").into_response()
}
