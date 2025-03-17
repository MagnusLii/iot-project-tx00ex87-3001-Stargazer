use crate::err::PasswordError;
#[cfg(feature = "pw_hash")]
use argon2::{
    password_hash::{rand_core::OsRng, PasswordHash, PasswordHasher, PasswordVerifier, SaltString},
    Argon2,
};

/// Generates a password hash using the Argon2 algorithm.
/// 
/// This function is available only when the `pw_hash` feature is enabled.
/// 
/// # Arguments
/// * `password` - The plaintext password to hash.
/// 
/// # Returns
/// * `Ok(String)` - The generated password hash in PHC string format.
/// * `Err(PasswordError)` - If hashing fails.
#[cfg(feature = "pw_hash")]
pub fn generate_phc_string(password: &str) -> Result<String, PasswordError> {
    let argon2 = Argon2::default();
    let salt = SaltString::generate(&mut OsRng);

    let phc_string = argon2
        .hash_password(password.as_bytes(), &salt)
        .map(|hash| hash.to_string())?;

    Ok(phc_string)
}

/// Verifies a password against its stored hash using Argon2.
/// 
/// This function is available only when the `pw_hash` feature is enabled.
/// 
/// # Arguments
/// * `password` - The plaintext password to verify.
/// * `hash` - The stored password hash in PHC string format.
/// 
/// # Returns
/// * `Ok(())` - If the password is verified successfully.
/// * `Err(PasswordError)` - If verification fails.
#[cfg(feature = "pw_hash")]
pub fn verify_password(password: &str, hash: &str) -> Result<(), PasswordError> {
    let argon2 = Argon2::default();
    let hash = PasswordHash::new(hash)?;

    let verified = argon2.verify_password(password.as_bytes(), &hash)?;

    Ok(verified)
}

/// Verifies a password against a stored plain-text password (for non-hashed authentication).
/// 
/// This function is available only when the `pw_hash` feature is disabled.
/// 
/// # Arguments
/// * `password` - The plaintext password to verify.
/// * `expected` - The expected password stored in plain text.
/// 
/// # Returns
/// * `Ok(())` - If the password matches.
/// * `Err(PasswordError::Other)` - If the password does not match.
#[cfg(not(feature = "pw_hash"))]
pub fn verify_password(password: &str, expected: &str) -> Result<(), PasswordError> {
    if password == expected {
        Ok(())
    } else {
        Err(PasswordError::Other)
    }
}

/// Checks whether a character is a valid input character for passwords.
/// 
/// This function ensures that only alphanumeric characters and ASCII punctuation are allowed.
/// 
/// # Arguments
/// * `c` - The character to validate.
/// 
/// # Returns
/// * `true` if the character is alphanumeric or an ASCII punctuation mark.
/// * `false` otherwise.
pub fn is_valid_char(c: char) -> bool {
    c.is_alphanumeric() || c.is_ascii_punctuation()
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
