# AGENTS.md

## Cursor Cloud specific instructions

This repo is **Docker-only**: all builds, unit tests, and integration tests run
inside `debian:trixie` containers driven by the top-level `Makefile`. There are
no host-level language manifests (no `package.json`, `requirements.txt`, etc.) —
dependencies live in `build/Dockerfile.build` / `build/Dockerfile.test` and in
Meson `.wrap` subprojects, all resolved at build time. See `README.md` and
`tests/README.md` for the canonical commands.

### Docker daemon (required, non-obvious)
- Docker is required for everything. The startup update script ensures `dockerd`
  is running and makes `/var/run/docker.sock` accessible to the `ubuntu` user, so
  `docker` / `make` work without `sudo`.
- If `docker ps` ever fails with a socket/permission error, start the daemon
  manually and fix the socket:
  `sudo bash -c 'nohup dockerd >/tmp/dockerd.log 2>&1 &' ; sleep 5 ; sudo chmod 666 /var/run/docker.sock`
- The VM runs Docker-in-Docker via `fuse-overlayfs` (set in `/etc/docker/daemon.json`)
  and `iptables-legacy`. Do not switch the storage driver back to `overlay2`.

### Build / test / run (standard commands)
- `make build` — builds both `.deb` packages into `target/debs/trixie/`. First
  run clones `openbmc/bmcweb` at the pinned commit and compiles it (large C++
  build, ~15 min on 4 cores); needs outbound network (GitHub). Subsequent
  `make build` runs re-clone/rebuild from clean.
- `make unit-test` — C++ gtest unit tests (fast, builder image only).
- `make test` — Redfish integration suite. Requires `.deb` artifacts from
  `make build` first, and runs the test container with `--cap-add SYS_ADMIN`
  and `--tmpfs /run/dbus`. The `make test` recipe `sudo chown`s
  `bmcweb/subprojects/packagecache` — passwordless `sudo` is available.
- There is no dedicated lint target; strictness is enforced at compile time
  (`werror=true`, `-Wall -Wextra`).

### Running the app live (for manual/end-to-end checks)
The integration container is normally `--rm`. To keep a live Redfish stack for
manual testing, run the test image directly with the bundled startup script:
```
docker run -d --cap-add SYS_ADMIN --tmpfs /run/dbus -p 8443:443 --name sonic-redfish-app \
  sonic-redfish-test:latest \
  bash -c 'bash tests/redfish-api/framework/start_services.sh && tail -f /dev/null'
```
This boots dbus → redis (CONFIG_DB db4, STATE_DB db6, seeded) → sonic-dbus-bridge
→ bmcweb on port 443 (mapped to host 8443). Query with basic auth `bmcweb:bmcweb`
over HTTPS (`curl -sk -u bmcweb:bmcweb https://localhost:8443/redfish/v1/`).
Power actions (`POST .../ComputerSystem.Reset`) and OEM alert/telemetry POSTs are
persisted as rows in Redis STATE_DB (db 6); inspect with
`docker exec sonic-redfish-app redis-cli -n 6 KEYS '*'`.
- `make test NODELETE=1` is the repo's built-in way to keep the test container
  alive (named `sonic-redfish-test-debug`) for debugging.
