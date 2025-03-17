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
Stargazer webserver

Usage: stargazer-ws [OPTIONS]

Options:
  -a, --address <ADDRESS>          Set server address
  -p, --port-http <PORT_HTTP>      Set HTTP port
  -s, --port-https <PORT_HTTPS>    Set HTTPS port
      --disable-http               Disable HTTP
      --enable-http                Enable HTTP
      --enable-http-api            Enable HTTP API if HTTP is disabled
      --disable-https              Disable HTTPS
      --enable-https               Enable HTTPS
      --enable-https-api           Enable HTTPS API if HTTPS is disabled
  -c, --config-file <CONFIG_FILE>  Set config file to use (default: config.toml)
  -w, --working-dir <WORKING_DIR>  Set working directory
      --db-dir <DB_DIR>            Set database directory
      --assets-dir <ASSETS_DIR>    Set assets directory
      --certs-dir <CERTS_DIR>      Set certificate directory
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

### Quick start instructions

You need to first create a key from the `Keys` page which can be accessed from the `Control` page before you can add commands to the queue.<br>
Generating a key is as simple as giving it a name then pressing generate.<br>
Then you can copy and give the key/token to the Stargazer device<br><br>

After that is done you can add commands from the `Control`/`Commands` page.<br>

The command options are as follows:<br>
* Target:<br>
`Sun`<br>`Moon`<br>`Mercury`<br>`Venus`<br>`Mars`<br>`Jupiter`<br>`Saturn`<br>`Uranus`<br>`Neptune`<br>
* Image Position:<br>
`Rising`: Takes a picture when the celestial object rises from the horizon.<br>
`Zenith`: Takes a picture at the zenith of the celestial object.<br>
`Setting`: Takes a picture when the celestial object close to setting beneath the horizon
`Any`: Takes a picture at the current given time.<br>

# API
The API currently has the following endpoints:

- `/api/upload` (POST)
- `/api/command` (GET, POST)
- `/api/diagnostics` (POST)
- `/api/time` (GET)

More details can be found in API.md

