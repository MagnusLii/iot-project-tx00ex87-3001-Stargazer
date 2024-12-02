# Requirements
- Rust compiler (tested on 1.82)
- sqlite3

# Test upload images
```
curl -X POST --data @base 127.0.0.1:7878/upload
```
where @base is a base64 encoded image
