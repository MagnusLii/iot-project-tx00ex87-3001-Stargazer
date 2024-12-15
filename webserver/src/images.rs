use std::{env, fs, path::PathBuf};

pub struct ImageDirectory {
    pub path: PathBuf,
    pub extensions: Vec<String>,
}

impl ImageDirectory {
    pub fn find_images(&self) -> Vec<PathBuf> {
        let mut images: Vec<PathBuf> = Vec::new();

        for entry in self.path.read_dir().unwrap() {
            let entry = entry.unwrap();
            let path = entry.path();
            if self
                .extensions
                .contains(&path.extension().unwrap().to_str().unwrap().to_string())
            {
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

// TODO: Maybe look in to using proper html templates
pub async fn update_gallery() -> bool {
    let tmp = env::temp_dir();
    let directory = ImageDirectory::default();
    let images = directory.find_images();

    let mut html = std::include_str!("../html/images.html").to_string();
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
