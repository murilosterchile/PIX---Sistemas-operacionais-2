# Makefile para Sistema de Transferência de Valores
# INF01151 - Sistemas Operacionais II - Trabalho Prático Etapa 2

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread

COMMON_DIR = common
SERVER_DIR = server
CLIENT_DIR = client
OBJ_DIR = obj

SERVER_TARGET = servidor
CLIENT_TARGET = cliente

COMMON_SOURCES = $(COMMON_DIR)/utils.cpp
COMMON_HEADERS = $(COMMON_DIR)/protocol.h $(COMMON_DIR)/utils.h \
                 $(COMMON_DIR)/server_data.h $(COMMON_DIR)/debug.h \
                 $(COMMON_DIR)/election.h

SERVER_SOURCES = $(SERVER_DIR)/servidor.cpp \
                 $(SERVER_DIR)/discovery.cpp \
                 $(SERVER_DIR)/processing.cpp \
                 $(SERVER_DIR)/interface.cpp \
                 $(SERVER_DIR)/election_manager.cpp
SERVER_HEADERS = $(SERVER_DIR)/discovery.h \
                 $(SERVER_DIR)/processing.h \
                 $(SERVER_DIR)/interface.h \
                 $(SERVER_DIR)/election_manager.h

CLIENT_SOURCES = $(CLIENT_DIR)/cliente.cpp \
                 $(CLIENT_DIR)/client_discovery.cpp \
                 $(CLIENT_DIR)/client_processor.cpp \
                 $(CLIENT_DIR)/client_interface.cpp
CLIENT_HEADERS = $(CLIENT_DIR)/client_discovery.h \
                 $(CLIENT_DIR)/client_processor.h \
                 $(CLIENT_DIR)/client_interface.h

COMMON_OBJECTS = $(COMMON_SOURCES:$(COMMON_DIR)/%.cpp=$(OBJ_DIR)/common_%.o)
SERVER_OBJECTS = $(SERVER_SOURCES:$(SERVER_DIR)/%.cpp=$(OBJ_DIR)/server_%.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(CLIENT_DIR)/%.cpp=$(OBJ_DIR)/client_%.o)

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(SERVER_TARGET): $(SERVER_OBJECTS) $(COMMON_OBJECTS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CLIENT_TARGET): $(CLIENT_OBJECTS) $(COMMON_OBJECTS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/common_%.o: $(COMMON_DIR)/%.cpp $(COMMON_HEADERS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/server_%.o: $(SERVER_DIR)/%.cpp $(SERVER_HEADERS) $(COMMON_HEADERS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/client_%.o: $(CLIENT_DIR)/%.cpp $(CLIENT_HEADERS) $(COMMON_HEADERS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(SERVER_TARGET) $(CLIENT_TARGET) *.o

.PHONY: all clean
