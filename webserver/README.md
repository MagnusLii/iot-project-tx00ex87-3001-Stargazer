# Requirements
- Rust compiler (tested on 1.82)
- sqlite3

# Test upload images
```
curl -H "Content-Type: application/json" --request POST --data @img.json 127.0.0.1:7878/api/upload
```
where @img.json contains:
```
{
    "key":"<api_key>",
    "data":"<base64_data>"
}
```
