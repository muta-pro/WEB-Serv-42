NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -MMD -MP

INC_DIR = includes
SRC_DIR = src
OBJ_DIR = obj
TEST_NAME = contract_tests

SRCS_FILES = main.cpp \
				config/ConfigParser.cpp \
				config/ServerConfig.cpp \
				http/HttpRequest.cpp \
				http/HttpRequestParser.cpp \
				http/HttpResponse.cpp \
				http/ResponseBuilder.cpp \
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
	rm -rf $(OBJ_DIR) $(TEST_NAME)

fclean: clean
	rm -f $(NAME)

re: fclean all

# Phase 0 is a contract/build baseline. This verifies that every public header
# can be included on its own; behavioral tests belong to the feature branches.
check-headers:
	@set -e; for header in $$(find $(INC_DIR) -name '*.hpp' -print | sort); do \
		printf '#include "%s"\nint main() {}\n' "$${header#$(INC_DIR)/}" | \
		$(CXX) $(CXXFLAGS) $(INCLUDES) -x c++ -fsyntax-only -; \
	done

$(TEST_NAME): tests/contracts.cpp src/http/HttpRequest.cpp src/http/HttpResponse.cpp src/network/Client.cpp src/cgi/CgiHandler.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

test: all check-headers $(TEST_NAME)
	./$(TEST_NAME)

.PHONY: all clean fclean re test check-headers
