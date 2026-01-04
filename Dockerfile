# Pixelc - Emscripten Build for Makapix Club
# Builds Pixelc to WebAssembly and serves via Caddy

# Build stage - Compile C code to WebAssembly via Emscripten
FROM emscripten/emsdk:3.1.51 AS builder

WORKDIR /app

# Copy source files
COPY . .

# Create web build directory and copy required files
RUN mkdir -p web && \
    cp index.html web/ && \
    cp -r icon/* web/ 2>/dev/null || true && \
    cp favicon.ico web/ 2>/dev/null || true && \
    cp -r res web/

# Compile with Emscripten
WORKDIR /app/web
RUN emcc -O3 \
    -I../include/ \
    -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s FULL_ES3=1 \
    -s EXPORTED_FUNCTIONS='["_main", "_e_io_idbfs_synced", "_e_io_file_upload_done"]' \
    -s EXPORTED_RUNTIME_METHODS=FS,ccall \
    -s SDL2_IMAGE_FORMATS='["png"]' \
    --preload-file ./res \
    -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY=1 -s EXIT_RUNTIME=1 \
    -lidbfs.js \
    -DPLATFORM_EMSCRIPTEN -DOPTION_GLES -DOPTION_SDL \
    ../src/e/*.c ../src/p/*.c ../src/r/*.c ../src/u/*.c ../src/*.c ../src/dialog/*.c ../src/tool/*.c \
    -o index.js

# Verify build succeeded
RUN test -f index.js && test -f index.wasm && echo "Build successful!"

# Production stage - Lightweight static file server
FROM caddy:2.8-alpine

# Copy built files from builder
COPY --from=builder /app/web /srv

# Copy Caddyfile
COPY Caddyfile /etc/caddy/Caddyfile

EXPOSE 80

# Caddy runs by default
