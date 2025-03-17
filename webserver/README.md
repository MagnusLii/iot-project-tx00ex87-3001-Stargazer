# Requirements
- Cargo/rustc (build, tested on version 1.84)

# Building

## Option A. Build only
```sh
cargo build --release
```
## Option B. Build and install to Cargo binary directory
```sh
cargo install
```

# Configuration
Server configuration can be set in a `config.toml` file which the program will look for in the current working directory by default.<br>

You can also specify a config file as an argument to the program.<br>

All available configuration options can be found in the <a href="example-config.toml">example-config.toml</a>

# Usage
```
Usage: stargazer-ws [OPTIONS]

Options:
  -a, --address <ADDRESS>          Server address
  -p, --port-http <PORT_HTTP>      HTTP port
  -s, --port-https <PORT_HTTPS>    HTTPS port
      --disable-http               HTTP disable
      --enable-http                HTTP enable
      --enable-http-api            HTTP enabled for API even if HTTP is otherwise disabled
      --disable-https              HTTPS disable
      --enable-https               HTTPS enable
      --enable-https-api           HTTPS enabled for API even if HTTPS is otherwise disabled
  -c, --config-file <CONFIG_FILE>  Config file path
  -w, --working-dir <WORKING_DIR>  Working directory
      --db-dir <DB_DIR>            Database directory
      --assets-dir <ASSETS_DIR>    Assets directory
      --certs-dir <CERTS_DIR>      Certificate directory
  -h, --help                       Print help
  -V, --version                    Print version
```

## Option A
#### You can run the server using
```sh
cargo run --
```
#### or by running the executable directly (located under target/<build_type>/)
```sh
cd target/release/
./stargazer-ws
```

## Option B
#### You can run the server from anywhere (provided that the Cargo bin dir is in your path)
```sh
stargazer-ws
```

## Web interface
When starting up the server for the first time you can login using the default credentials:<br>
Username: admin<br>
Password: admin<br>

* You can change the username and/or password of the admin account on the user page, which you can get to by clicking the account name in the navigation bar/menu.<br>

# API
The API currently has the following endpoints:

- `/api/upload` (POST)
- `/api/command` (GET, POST)
- `/api/diagnostics` (POST)
- `/api/time` (GET)

More details can be found in API.md

