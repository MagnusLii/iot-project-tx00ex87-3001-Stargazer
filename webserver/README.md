# Requirements
- Rust compiler (tested on 1.823
- sqlite3

# Test upload images
```
curl -H "Content-Type: application/json" --request POST --data @img.json 127.0.0.1:7878/api/upload
```
where @img.json contains:
```
{
    "toke":"<api_token>",
    "data":"<base64_data>"
}
```
