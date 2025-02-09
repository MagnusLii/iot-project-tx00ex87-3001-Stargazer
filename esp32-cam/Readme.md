

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
- Set `Long filename support` to `long filename buffer in heap` and lenght to 255.
- Set `configTimer_TASK_STACK_DEPTH` from `2048` -> `4096`
- Set `Partition Table` to `Custom partition table CSV` and use `partitions.csv` as the table.
- Set `CONFIG_MAIN_TASK_STACK_SIZE` from `3584` to `8192` when using memory intentsive tests.

### Code notes

`.jpeg_quality = 8` Is the lowest it can go while staying stable, 7 works mostly, 6 will fail most times.  
^^ this needs to be tested again, I've done code improvements.