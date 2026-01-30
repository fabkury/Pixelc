# Pixelc - Emscripten Build for Makapix Club
# Builds Pixelc to WebAssembly and serves via Caddy

# Build stage - Compile C code to WebAssembly via Emscripten
FROM emscripten/emsdk:3.1.61 AS builder

WORKDIR /app

# Download and build libwebp for Emscripten
RUN curl -L https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.3.2.tar.gz -o libwebp.tar.gz && \
    tar xzf libwebp.tar.gz && \
    cd libwebp-1.3.2 && \
    emconfigure ./configure --disable-shared --enable-static \
        --disable-threading --disable-gl --disable-sdl \
        --disable-png --disable-jpeg --disable-tiff --disable-gif \
        --enable-libwebpmux --enable-libwebpdemux && \
    emmake make -j$(nproc) && \
    emmake make install

# Download and build giflib for Emscripten
RUN curl -L https://sourceforge.net/projects/giflib/files/giflib-5.2.2.tar.gz/download -o giflib.tar.gz && \
    tar xzf giflib.tar.gz && \
    cd giflib-5.2.2 && \
    emcc -O3 -c dgif_lib.c gif_err.c gifalloc.c gif_hash.c openbsd-reallocarray.c -I. && \
    emar rcs libgif.a *.o && \
    cp libgif.a /usr/local/lib/ && \
    cp gif_lib.h /usr/local/include/

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
    -I/usr/local/include \
    -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s FULL_ES3=1 \
    -s EXPORTED_FUNCTIONS='["_main", "_e_io_idbfs_synced", "_e_io_file_upload_done", "_pixelc_load_webp_data", "_pixelc_load_gif_data", "_pixelc_load_bmp_data", "_pixelc_load_png_data", "_malloc", "_free"]' \
    -s EXPORTED_RUNTIME_METHODS=FS,ccall \
    -s SDL2_IMAGE_FORMATS='["png","bmp"]' \
    --preload-file ./res \
    -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY=1 -s EXIT_RUNTIME=1 \
    -lidbfs.js \
    -L/usr/local/lib -lwebpmux -lwebpdemux -lwebp -lsharpyuv -lgif -lm \
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
