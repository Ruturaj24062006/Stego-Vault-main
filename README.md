# Stego Vault

## Start the project on Windows

1. Start Docker Desktop first. If the Docker service is not running, any `docker build` or `docker run` command will fail with a pipe/daemon error.
2. From the repository root, build and start the backend with Docker Compose:

```powershell
docker compose up --build -d
```

3. Start the frontend in a second terminal:

```powershell
npm install
npm run dev
```

## Manual Docker commands

If you prefer running the backend without Compose, use these commands from the repository root:

```powershell
docker build -t stego-vault-backend ./backend
docker run -d --name stego-vault-backend --restart unless-stopped -p 8080:8080 stego-vault-backend
```

The frontend expects the backend at `http://localhost:8080`.