CC = g++
CPPFLAGS = -std=c++17 -I$(HOME)/.brew/include -DGL_SILENCE_DEPRECATION -O3 # -g -fsanitize=address # -Wall -Wextra -Werror
LDFLAGS = -lGLFW -framework OpenGL -L$(HOME)/.brew/lib

NAME = scop

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)


$(NAME): $(OBJS)
	@echo Comiling $(NAME) executable
	$(CC) $(CPPFLAGS) $(OBJS) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

prune: fclean
	rm -rf ./lib-*
	rm -rf ./include-*
	rm -rf ./raylib/raylib

re: fclean $(NAME)

.PHONY: all clean fclean re
