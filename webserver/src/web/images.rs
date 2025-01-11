use std::{env, fs, path::PathBuf};

#[derive(Clone)]
pub struct ImageDirectory {
    pub path: PathBuf,
    pub extensions: Vec<String>,
}

impl ImageDirectory {
    pub fn new(path: PathBuf, extensions: Vec<String>) -> Self {
        Self { path, extensions }
    }

    pub fn find_images(&self) -> Vec<PathBuf> {
        let mut images: Vec<PathBuf> = Vec::new();

        for entry in self.path.read_dir().unwrap() {
            let entry = entry.unwrap();
            let path = entry.path();
            if self
                .extensions
                .contains(&path.extension().unwrap().to_str().unwrap().to_string())
            {
                //println!("Found image: {}", path.display());
                images.push(path);
            }
        }
        images
    }
}

impl Default for ImageDirectory {
    fn default() -> Self {
        Self {
            path: PathBuf::from("./assets/images"),
            extensions: vec![
                String::from("png"),
                String::from("jpg"),
                String::from("jpeg"),
                String::from("webp"),
                String::from("gif"),
                String::from("svg"),
            ],
        }
    }
}

fn generate_image_url(path: &PathBuf) -> String {
    format!(
        "/assets/images/{}",
        path.file_name().unwrap().to_str().unwrap()
    )
}

pub fn generate_html(images: Vec<PathBuf>) -> String {
    let mut html = String::new();
    for image in images {
        html += &format!("<img src=\"{}\" />\n", generate_image_url(&image));
    }
    html
}

pub async fn update_gallery() -> bool {
    let tmp = env::temp_dir();
    let directory = ImageDirectory::default();
    let images = directory.find_images();

    let mut html = std::include_str!("../../html/images.html").to_string();
    let html_images = images
        .iter()
        .map(|image| format!("<img src=\"{}\"/>", image.display().to_string()))
        .collect::<Vec<String>>()
        .join("\n");
    html = html.replace("<!--IMAGES-->", &html_images);

    let result = match fs::write(&tmp.join("sgwebserver/images.html"), html) {
        Ok(_) => true,
        Err(e) => {
            eprintln!("Error updating images: {}", e);
            false
        }
    };

    result
}

pub async fn register_image(db: &sqlx::SqlitePool, name: &str, path: &str, command_id: i64) {
    let sql = "INSERT INTO images (name, path, command_id) VALUES (?, ?, ?)";
    sqlx::query(sql)
        .bind(name)
        .bind(path)
        .bind(command_id)
        .execute(db)
        .await
        .unwrap();
}

pub async fn unregister_image(db: &sqlx::SqlitePool, path: &str) {
    let sql = "DELETE FROM images WHERE path = ?";
    sqlx::query(sql).bind(path).execute(db).await.unwrap();
}

#[derive(sqlx::FromRow)]
pub struct Image {
    id: i64,
    pub name: String,
    pub path: String,
    command_id: i64,
}

async fn get_image_list(db: &sqlx::SqlitePool) -> Result<Vec<Image>, sqlx::Error> {
    let sql = "SELECT * FROM images";
    let images = sqlx::query_as(sql).fetch_all(db).await?;

    Ok(images)
}

pub async fn check_images(
    db: &sqlx::SqlitePool,
    dir: &ImageDirectory,
    update: bool,
) -> Result<(), Box<dyn std::error::Error>> {
    let images = dir.find_images();

    for image in &images {
        println!("Image: {}", image.display());
    }

    // Verify that all images in the database are present in the directory
    match get_image_list(&db).await {
        Ok(db_images) => {
            for db_image in &db_images {
                if !images.contains(&PathBuf::from(&db_image.path)) {
                    eprintln!("Image {} not found in directory", db_image.path);
                    //unregister_image(&db, &db_image.path).await;
                }
            }

            if update {
                update_images(&db, images, db_images).await;
            }
        }
        Err(e) => {
            eprintln!("Error getting image list from database: {}", e);
        }
    }

    Ok(())
}

async fn update_images(db: &sqlx::SqlitePool, dir_images: Vec<PathBuf>, db_images: Vec<Image>) {
    for image in &dir_images {
        let path = image.to_str().expect("Failed to get path");

        // Check if image is already registered
        if db_images.iter().any(|i| i.path == path) {
            continue;
        }

        // Look for the correct format <date>-<id>-<name>.<ext>
        let filename = image.file_name().unwrap().to_str().unwrap();

        // Remove extension
        let filename = filename.split(".").next().unwrap();

        // Collect parts
        let parts = filename.split("-").collect::<Vec<&str>>();
        if parts.len() != 3 {
            continue;
        }

        // Attempt to parse date and id
        let date = parts[0].parse::<i64>().ok();
        let id = parts[1].parse::<i64>().ok();

        if date.is_none() || id.is_none() {
            continue;
        }

        let name = parts[2];

        register_image(db, name, path, id.unwrap()).await;
    }
}

pub async fn get_image_info(
    db: &sqlx::SqlitePool,
    mut page: u32,
    page_size: u32,
) -> Result<Vec<Image>, sqlx::Error> {
    if page == 0 {
        page = 1
    }
    let offset = (page - 1) * page_size;
    let sql = "SELECT * FROM images LIMIT ? OFFSET ?";
    let images = sqlx::query_as(sql)
        .bind(page_size)
        .bind(offset)
        .fetch_all(db)
        .await?;

    if images.len() == 0 {
        println!("No images found");
        return Ok(vec![]);
    }

    Ok(images)
}
