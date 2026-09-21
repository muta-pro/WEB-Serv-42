NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++20 -MMD -MP

INC_DIR = includes
SRC_DIR = src
OBJ_DIR = obj

SRCS_FILES = main.cpp \
				config/ConfigParser.cpp \
				http/HttpRequest.cpp \
				http/HttpResponse.cpp \
				network/ServerManager.cpp \
				network/Socket.cpp \
				network/Client.cpp \
				cgi/CgiHandler.cpp

SRCS = $(addprefix $(SRC_DIR)/, $(SRCS_FILES))
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

INCLUDES = -I $(INC_DIR)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "Built $(NAME) successfuly. Ducks are tasty btw."

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re