IDIR = ./include
CXX = clang++
CFLAGS = -I$(IDIR) \
		 -I./third_party/boost_1_83_0 \
		 -Wno-logical-op-parentheses

ODIR = obj

LIBS =

_DEPS =
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = expression-parser.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

.PHONY: all clean
.SECONDARY: main-build

all: pre-build main-build

main-build: expression-parser

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

expression-parser: $(OBJ)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

pre-build:
	@if [ ! -d "./third_party/boost_1_83_0" ] ; then                                              \
		echo "INFO: Downloading boost libraries";                                                   \
		wget https://boostorg.jfrog.io/artifactory/main/release/1.83.0/source/boost_1_83_0.tar.bz2; \
		tar --bzip2 -xf boost_1_83_0.tar.bz2;                                                       \
		mkdir third_party;                                                                          \
		mv boost_1_83_0 third_party/;                                                               \
		rm boost_1_83_0.tar.bz2;                                                                    \
	else                                                                                          \
		echo "INFO: No need to download boost libraries";                                           \
	fi

clean:
	rm -f $(ODIR)/*.o expression-parser
