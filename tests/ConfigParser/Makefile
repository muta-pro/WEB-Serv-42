# webserv - config parser tests
#
#	make              build, then run every section
#	make help         everything else
#
# from the repo root:  make -C tests/ConfigParser
# it has its own Makefile so the shared one at the root stays untouched.

NAME		:= tester
REPO		:= $(abspath ../..)

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++17 -g
CPPFLAGS	:= -I$(REPO)/includes -I. -DREPO_ROOT='"$(REPO)"' -MMD -MP
LDFLAGS		:=

# SAN=0 builds without AddressSanitizer + UBSan: faster, but it misses
# out-of-bounds reads and undefined behaviour that would otherwise fail a test.
SAN			?= 1
ifeq ($(SAN),1)
SANITIZERS	:= -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
CXXFLAGS	+= $(SANITIZERS)
CPPFLAGS	+= -DTESTER_SANITIZERS
LDFLAGS		+= $(SANITIZERS)
endif

OBJ_DIR		:= build

# main.cpp #includes src/config/ConfigParser.cpp itself, to reach its static
# functions (see parser_under_test.hpp), so ConfigParser.cpp is NOT listed.
SRCS		:= main.cpp framework/Tester.cpp framework/Checks.cpp
REPO_SRCS	:= src/config/ServerConfig.cpp
OBJS		:= $(SRCS:%.cpp=$(OBJ_DIR)/%.o) $(REPO_SRCS:%.cpp=$(OBJ_DIR)/repo/%.o)
DEPS		:= $(OBJS:.o=.d)

# one target per section, read off the file names: sections/05_values.hpp -> values
SECTIONS	:= $(foreach f,$(sort $(notdir $(wildcard sections/*.hpp))),$(word 2,$(subst _, ,$(basename $(f)))))

# the flags are written to a file; when they change (SAN=0), everything rebuilds
FLAGS_FILE	:= $(OBJ_DIR)/.flags
FLAGS_NOW	:= $(CXX) $(CXXFLAGS) $(LDFLAGS) $(REPO)

ifdef NO_COLOR
C_RESET		:=
C_BOLD		:=
C_DIM		:=
C_CYAN		:=
C_GREEN		:=
else
C_RESET		:= \033[0m
C_BOLD		:= \033[1m
C_DIM		:= \033[2m
C_CYAN		:= \033[36m
C_GREEN		:= \033[32m
endif

all: test

build: $(NAME)

$(NAME): $(OBJS)
	@printf "$(C_CYAN)  link     $(C_RESET)%s\n" "$@"
	@$(CXX) $(LDFLAGS) $(OBJS) -o $@
	@printf "$(C_GREEN)  built    $(C_RESET)./%s $(C_DIM)(SAN=$(SAN))$(C_RESET)\n" "$@"

# main.o is where the parser itself gets compiled, so say so
$(OBJ_DIR)/main.o: SHOWN = main.cpp $(C_DIM)+ src/config/ConfigParser.cpp$(C_RESET)

$(OBJ_DIR)/repo/%.o: $(REPO)/%.cpp $(FLAGS_FILE)
	@mkdir -p $(@D)
	@printf "$(C_CYAN)  compile  $(C_RESET)%s\n" "$*.cpp"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp $(FLAGS_FILE)
	@mkdir -p $(@D)
	@printf "$(C_CYAN)  compile  $(C_RESET)%b\n" "$(or $(SHOWN),$<)"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(FLAGS_FILE): FORCE
	@mkdir -p $(@D)
	@if [ "$$(cat $@ 2>/dev/null)" != '$(FLAGS_NOW)' ]; then echo '$(FLAGS_NOW)' > $@; fi

FORCE:

-include $(DEPS)

# ---------------------------------------------------------------- running

test: $(NAME)
	@./$(NAME) $(ARGS)

$(SECTIONS): $(NAME)
	@./$(NAME) $@ $(ARGS)

verbose: $(NAME)
	@./$(NAME) --verbose $(ARGS)

quiet: $(NAME)
	@./$(NAME) --quiet $(ARGS)

list: $(NAME)
	@./$(NAME) --list

facts: $(NAME)
	$(if $(FILE),,$(error which file? make facts FILE=conf/default.conf))
	@./$(NAME) --facts "$(FILE)"

help:
	@printf "\n$(C_BOLD)  config parser tests$(C_RESET)\n\n"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make" "build, then run every section"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make <section>" "run one section: $(SECTIONS)"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make verbose" "every test with its input, expected and got"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make quiet" "one mark per test, details only for failures"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make list" "the sections and how many tests each has"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make facts FILE=<conf>" "what the parser makes of a config, as facts"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make build" "build, don't run"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "make clean / fclean / re" "remove build/, also ./$(NAME), rebuild"
	@printf "\n$(C_BOLD)  add to any of them$(C_RESET)\n\n"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "ARGS=\"...\"" "options for ./$(NAME), e.g. ARGS=\"-f listen -x\""
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "SAN=0" "build without AddressSanitizer + UBSan"
	@printf "  $(C_CYAN)%-26s$(C_RESET)%s\n" "NO_COLOR=1" "no colours"
	@printf "\n  ./$(NAME) --help lists the options ARGS can take.\n\n"

clean:
	@rm -rf $(OBJ_DIR)
	@printf "$(C_DIM)  removed $(OBJ_DIR)/$(C_RESET)\n"

fclean: clean
	@rm -f $(NAME)
	@printf "$(C_DIM)  removed $(NAME)$(C_RESET)\n"

re: fclean
	@$(MAKE) --no-print-directory all

.PHONY: all build test verbose quiet list facts help clean fclean re FORCE $(SECTIONS)
