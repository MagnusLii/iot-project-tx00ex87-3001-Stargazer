use crate::err::PasswordError;
#[cfg(feature = "pw_hash")]
use argon2::{
    password_hash::{rand_core::OsRng, PasswordHash, PasswordHasher, PasswordVerifier, SaltString},
    Argon2,
};

#[cfg(feature = "pw_hash")]
pub fn generate_phc_string(password: &str) -> Result<String, PasswordError> {
    let argon2 = Argon2::default();
    let salt = SaltString::generate(&mut OsRng);

    let phc_string = argon2
        .hash_password(password.as_bytes(), &salt)
        .map(|hash| hash.to_string())?;

    Ok(phc_string)
}

#[cfg(feature = "pw_hash")]
pub fn verify_password(password: &str, hash: &str) -> Result<(), PasswordError> {
    let argon2 = Argon2::default();
    let hash = PasswordHash::new(hash)?;

    let verified = argon2.verify_password(password.as_bytes(), &hash)?;

    Ok(verified)
}

#[cfg(not(feature = "pw_hash"))]
pub fn verify_password(password: &str, expected: &str) -> Result<(), PasswordError> {
    if password == expected {
        Ok(())
    } else {
        Err(PasswordError::Other)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_password_hash() {
        let password = "password123";
        let hash = generate_phc_string(password).unwrap();
        assert!(verify_password(password, &hash).is_ok());
    }

    #[test]
    fn test_password_hash_invalid() {
        let password = "password123";
        let hash = "invalid_hash";
        assert!(verify_password(password, hash).is_err());
    }
}
