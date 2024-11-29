use std::path::PathBuf;

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
            path: PathBuf::from("./assets"),
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

pub fn generate_image_url(path: &PathBuf) -> String {
    format!("/assets/{}", path.file_name().unwrap().to_str().unwrap())
}

pub fn generate_html(images: Vec<PathBuf>) -> String {
    let mut html = String::new();
    for image in images {
        html += &format!("<img src=\"{}\" />\n", generate_image_url(&image));
    }
    html
}
