#!/bin/sh
# Run the Vite dev server with Node 20 (the system default is Node 14).
export PATH="$HOME/.nvm/versions/node/v20.19.6/bin:$PATH"
cd "$(dirname "$0")/../frontend" && exec npm run dev
