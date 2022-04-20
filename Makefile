NAME=debase

CXX = g++

CXXFLAGS = -std=c++17 -O0 -g3 -Wall $(INCDIRS)

OBJECTS=                            \
	lib/c25519/src/sha512.o         \
	lib/c25519/src/edsign.o         \
	lib/c25519/src/ed25519.o        \
	lib/c25519/src/fprime.o         \
	lib/c25519/src/f25519.o         \
	src/OpenURL-Linux.o             \
	src/ProcessPath-Linux.o         \
	src/machine/Machine-Linux.o     \
	src/state/StateDir-Linux.o      \
	src/ui/View.o                   \
	src/main.o                      \

LIBS =                              \
	-lgit2                          \
	-lcurl                          \
	-lpthread                       \
	-lz                             \
	-lformw                         \
	-lmenuw                         \
	-lpanelw                        \
	-lncursesw                      \

LIBDIRS =                           \
	-L./lib/libgit2/build-linux     \
	-L./lib/libcurl/build/lib       \
	-L./lib/ncurses/lib             \

INCDIRS =                           \
	-isystem ./lib/ncurses/include  \
    -iquote ./lib/libgit2/include   \
	-iquote ./src                   \
	-iquote .                       \

all: ${OBJECTS}
	$(CXX) $(CXXFLAGS) $? -o $(NAME) $(LIBDIRS) $(LIBS)

clean:
	rm -f *.o $(OBJECTS) $(NAME)
