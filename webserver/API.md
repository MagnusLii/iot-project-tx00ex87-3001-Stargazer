# API
The API currently has the following endpoints:

- `/api/upload` (POST)
- `/api/command` (GET, POST)
- `/api/diagnostics` (POST)
- `/api/time` (GET)

## `/api/upload`
Uploads an image wrapped in JSON to the webserver.

The JSON should contain the following fields:

- `token`: The API token (string)
- `id`: The ID of the command (int)
- `data`: The base64 encoded image (string)

Example:
```
curl -H "Content-Type: application/json" --request POST --data @img.json 127.0.0.1:7878/api/upload
```
where @img.json contains:
```
{
    "token":"<api_token>",
    "id":<id>,
    "data":"<base64_data>"
}
```

## `/api/command`
### GET
Fetch a command from the webserver.

Example:
```
curl "127.0.0.1:7878/api/command?token=<api_token>"
```

Returns a JSON object with the following fields:
- `target`: The target object (int)
- `id`: The ID of the command/picture (int)
- `position`: The position to take the photo (int) {1,2,3}

Or an empty object if no command is available

### POST
Send a response in JSON to the webserver.

The JSON should contain the following fields:

- `token`: The API token (string)
- `id`: The ID of the command (int)
- `status`: The command status (int)

Optional fields:
- `time`: The estimated time to take the photo (int) [NOTE: unix epoch time sent along with status 2]

Example:
```
curl -H "Content-Type: application/json" --request POST --data "{\"token\":\"ae07ddd0-5dc1-47d2-98ab-9cea274f1f61\",\"id\":1,\"response\":false}" 127.0.0.1:7878/api/command
```

## `/api/diagnostics`
Send diagnostics to the webserver.

[NOTE: TBD] The JSON should contain the following fields:

- `token`: The API token (string)
- `status`: The status (string)
- `message`: The message (string)

## `/api/time`
Get the current time from the webserver.

Example:
```
curl "127.0.0.1:7878/api/time"
```

Returns time in seconds since epoch

