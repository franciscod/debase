## Build for macOS

### Build libgit2

    cd lib/libgit2
    mkdir build-macos && cd build-macos
    cmake -DBUILD_SHARED_LIBS=OFF -DUSE_HTTPS=OFF ..
    make clean ; make git2 -j8

### Build ncurses

    cd lib/ncurses
    ./configure --prefix=/usr --enable-widec
    make -j8

### Build libcurl

    cd lib/libcurl
    ./configure --prefix=$PWD/build --disable-shared --with-secure-transport --disable-debug --disable-curldebug --disable-ftp --disable-file --disable-ldap --disable-ldaps --disable-rtsp --disable-proxy --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --disable-mqtt --disable-manual --disable-verbose --disable-crypto --disable-alt --without-brotli --without-zstd --without-libpsl --without-libgsasl --without-librtmp --without-winidn
    make install-strip

### Build debase

- Open debase.xcodeproj
- Build



## Build for Linux

### Build libgit2



### Build ncurses



### Build debase

    cd debase
    make
