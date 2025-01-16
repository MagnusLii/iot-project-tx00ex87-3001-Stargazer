#[cfg(feature = "pw_hash")]
use argon2::{
    password_hash::{rand_core::OsRng, PasswordHash, PasswordHasher, PasswordVerifier, SaltString},
    Argon2,
};

#[cfg(feature = "pw_hash")]
pub fn generate_phc_string(password: &str) -> String {
    let argon2 = Argon2::default();
    let salt = SaltString::generate(&mut OsRng);

    argon2
        .hash_password(password.as_bytes(), &salt)
        .map(|hash| hash.to_string())
        .expect("Error hashing password")
}

#[cfg(feature = "pw_hash")]
pub fn verify_password(password: &str, hash: &str) -> Result<(), Box<dyn std::error::Error>> {
    let argon2 = Argon2::default();
    let hash = PasswordHash::new(hash).expect("Error parsing password hash");

    let verified = argon2.verify_password(password.as_bytes(), &hash)?;

    Ok(verified)
}

#[cfg(not(feature = "pw_hash"))]
pub fn verify_password(password: &str, expected: &str) -> Result<(), Box<dyn std::error::Error>> {
    if password == expected {
        Ok(())
    } else {
        Err("Invalid password".into())
    }
}
