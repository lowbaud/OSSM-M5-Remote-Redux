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

To preview the development flasher too, offer the same build as one or more
development branches after `release_build.py`, then start the preview as above.
Without `--branch`, the current branch is listed.

```sh
python scripts/stage_development_preview.py --branch develop --branch test-branch
```

Open <http://localhost:4000/flash/development/>. Commit links in the preview
point at GitHub, so commits that only exist locally show a 404 page there.
