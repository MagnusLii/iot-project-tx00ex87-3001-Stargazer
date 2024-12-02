

# Setup stuff and notes... idk what to call this...

### ESP-IDF setup pre-build

- Add a dependency on `espressif/esp32-camera` component:
  ```bash
  idf.py add-dependency "espressif/esp32-camera"
  ```
  (or add it manually in idf_component.yml)

- Enable PSRAM in `menuconfig` 
- Set Flash and PSRAM frequiencies to 80MHz (Flash speed must be set before PSRAM speed can be changed)
- Set Flash size to 4MB