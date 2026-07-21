# Documentation site

The local preview runs Jekyll in Docker, so only Docker Compose is required.

To preview the web flasher, first build the firmware files and local release
metadata from the repository root. Skip this step when previewing only the
documentation.

```sh
python release_build.py
python scripts/write_release_info.py
```

Start the local site preview with Docker Compose:

```sh
cd docs
docker compose up
```

Open <http://localhost:4000/> or <http://localhost:4000/flash/>. Press `Ctrl+C`
to stop the preview.
