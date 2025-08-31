# NRest

NRest is a lightweight and unofficial REST API for [n8n](https://n8n.io)®, written in C using [Ulfius](https://babelouest.github.io/ulfius) and [SQLite](https://sqlite.org).

This project tries to imitate the same functionality of the official n8n API, hosted in [api.n8n.io](https://api.n8n.io), but with limited features.

The main goal is to provide custom n8n workflow templates using my own endpoint while being lightweight in memory usage.

**Note**: this is an *experimental* project. Please do not run it in production critical businesses, since I do not offer any support. You may want to consider [strapi](https://strapi.io) (since the official n8n API uses it) before using this.

Tested against version 1.107.4, I cannot make sure whether earlier versions or later versions were or will be supported since the n8n API is subject to change.

## Dependencies

Install these packages on Debian/Ubuntu-based systems:

```sh
sudo apt update && sudo apt install -y \
  build-essential \
  libulfius-dev \
  libjansson-dev \
  libsqlite3-dev
```

## Build

To build the project, run the following command:

```sh
make debug
```

Optionally, you may want to use [`watchexec`](https://github.com/watchexec/watchexec) to automatically rebuild on changes:

```sh
watchexec -e c -- make debug
```

To build the project in release mode, run the following command:

```sh
make release
```

To run the project, regardless of the build mode:

```sh
make run
```

## Usage

The following endpoints are implemented:
* `GET /health` -- Check if the API is running.
* `GET /templates/categories` -- Retrieve all workflow categories.
* `GET /templates/collections` -- Retrieve all workflow collections.
* `GET /templates/collections/:id` -- Get a specific collection by ID.
* `GET /templates/search` -- Search for workflow templates.
* `GET /templates/workflows` -- Retrieve all workflow templates.
* `GET /templates/workflows/:id` -- Get a specific workflow template by ID.

CORS Preflight Support is also implemented via the OPTIONS header:
* `OPTIONS /templates/categories`
* `OPTIONS /templates/collections`
* `OPTIONS /templates/collections/:id`
* `OPTIONS /templates/search`
* `OPTIONS /templates/workflows`
* `OPTIONS /templates/workflows/:id`
  
The following internal endpoints are implemented:
* `GET /workflows/templates/:id` -- Get a specific workflow template by ID.

For easy of use, the following custom endpoints are implemented:
* `PUT /templates/workflows` -- Create new workflow.
* `PUT /templates/collections` -- Create new collection of workflows.
* `PATCH /templates/collections` -- Insert new template workflow into a collection.

<br>

You can test endpoints using [curl](https://curl.se) or any other HTTP client.

```sh
curl http://localhost:8080/workflows
```

## Samples
The following samples are provided:
* **Database samples**: You may want to run `scripts/test-data.sh` while the REST API server is running in order to insert custom sample data.
* **Unit testing samples (from upstream)**: You may want to look at the mock directory. `627X` are sample workflows and `6270_imported.json` is used for testing the internal endpoint. `n8n-api-collections.json` is used for checking collection JSON structure.
* **Configuration samples**: An NGINX and systemd sample configuration files are provided in the conf folder. You may want to replace @SERVER_NAME@ by your REST API server name and @N8N_SERVER_NAME@ by your N8N server name. Certbot must also be installed and configured in your server.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Contributing

Contributions are welcome. You may want to take a look at the `TODO` file.

## References

* [Ulfius Web Framework](https://babelouest.github.io/ulfius)
* [Jansson JSON Library](https://github.com/akheron/jansson)
* [SQLite](https://sqlite.org)
* [watchexec](https://github.com/watchexec/watchexec)

---

[n8n](https://n8n.io)® is a registered trademark. This project is not affiliated with or endorsed by to n8n or n8n.io in any way. This is an independent implementation for educational and interoperability purposes only.
